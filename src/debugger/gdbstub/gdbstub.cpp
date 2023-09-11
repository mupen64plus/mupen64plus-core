#include <SDL.h>
#include <SDL_net.h>
#include <numeric>
#include <string>
#include "gdbstub.h"
#include "api/callbacks.h"
#include "../dbg_debugger.h"
#include "../dbg_breakpoints.h"
#include "device/r4300/tlb.h"
#include "main/main.h"

#define BUFFER_SIZE 8192

char gdb_buffer[BUFFER_SIZE];

char gdb_send_buffer[BUFFER_SIZE];

TCPsocket gdb_socket;

auto sendLength = 0;

int gdb_loop(void *x);
void gdb_send_signal_stop();
void set_checksum_and_sendlength(int hash_index);
uint32_t DebugVirtualToPhysical(uint32_t address);

void gdbstub_init() {
#if SDL_VERSION_ATLEAST(2,0,0)
    SDL_CreateThread(gdb_loop, "gdb_loop", NULL);
#else
    SDL_CreateThread(gdb_loop, NULL);
#endif
}

int gdb_loop(void *x) {
	if(SDL_Init(0) == -1) {
        DebugMessage(M64MSG_ERROR, "Couldn't initialize SDL: %s", SDL_GetError());
		return M64ERR_SYSTEM_FAIL;
	}
	if(SDLNet_Init() == -1) {
		DebugMessage(M64MSG_ERROR, "Gdb stub: Could not initialize SDL Net library");
		return M64ERR_SYSTEM_FAIL;
	}

    IPaddress serverIP;
    SDLNet_ResolveHost(&serverIP, NULL, 5555);
    auto serverSocket = SDLNet_TCP_Open(&serverIP);
    if(serverSocket == nullptr) {
        DebugMessage(M64MSG_ERROR, "Gdb stub: Server socket creation failed");
        return M64ERR_SYSTEM_FAIL;
    }

    while(true)
    {
        gdb_socket = SDLNet_TCP_Accept(serverSocket);

        while(gdb_socket != nullptr)
        {
            auto recvLength = SDLNet_TCP_Recv(gdb_socket, gdb_buffer, BUFFER_SIZE - 1);
            if(recvLength <= 0) {
                DebugMessage(M64MSG_ERROR, "Gdb stub: TCP receive error");
                break;
            }

            auto& qSupported = "qSupported";
            if(gdb_buffer[0] == '-') {
                sendLength = 0;
            } else if(gdb_buffer[0] == '+') {
                gdb_send_buffer[0] = '+';
                sendLength = 1;
            } else if(gdb_buffer[0] == '\x03') {
                g_dbg_runstate = M64P_DBG_RUNSTATE_PAUSED;
            } else if(memcmp(gdb_buffer + 1, qSupported, sizeof(qSupported) - 1) == 0) {
                auto& supported = "$PacketSize=1400;hwbreak+;";
                strncpy(gdb_send_buffer, supported, sizeof(supported) - 1);
                set_checksum_and_sendlength(sizeof(supported) - 1);
            } else if(gdb_buffer[1] == 'g') {
                auto regs = r4300_regs(&g_dev.r4300);
                gdb_send_buffer[0] = '$';
                for(auto i = 0; i < 32; i++) {
                    snprintf(gdb_send_buffer + 1 + i * 16, 17, "%016" SCNx64, regs[i]);
                }

                auto cp0reg = r4300_cp0_regs(&g_dev.r4300.cp0);
                snprintf(gdb_send_buffer + 1 + 32 * 16, 17, "%016" SCNx64, (uint64_t) cp0reg[CP0_STATUS_REG]);
                snprintf(gdb_send_buffer + 1 + 33 * 16, 17, "%016" SCNx64, (uint64_t) g_dev.r4300.lo);
                snprintf(gdb_send_buffer + 1 + 34 * 16, 17, "%016" SCNx64, (uint64_t) g_dev.r4300.hi);
                snprintf(gdb_send_buffer + 1 + 35 * 16, 17, "%016" SCNx64, (uint64_t) cp0reg[CP0_BADVADDR_REG]);
                snprintf(gdb_send_buffer + 1 + 36 * 16, 17, "%016" SCNx64, (uint64_t) cp0reg[CP0_CAUSE_REG]);
                snprintf(gdb_send_buffer + 1 + 37 * 16, 17, "%016" SCNx64, (uint64_t) *r4300_pc(&g_dev.r4300));

                const auto reg_len = 8 * 2;
                const auto non_gen_reg_count = 6;
                const auto non_gen_reg_total_size = reg_len * non_gen_reg_count;

                const auto gen_reg_count = 32;
                const auto gen_reg_total_size = reg_len * gen_reg_count;
                int message_end = 1 + gen_reg_total_size + non_gen_reg_total_size;

                set_checksum_and_sendlength(message_end);
            } else if(gdb_buffer[1] == 'G') {
                auto regs = r4300_regs(&g_dev.r4300);

                for(auto i = 0; i < 32; i++) {
                    uint64_t data;
                    char buf[17];
                    strncpy(buf, gdb_buffer + 2 + i * 16, sizeof(buf) - 1);
                    buf[16] = '\0';
                    sscanf(buf, "%" SCNx64, &data);
                    regs[i] = data;
                }

                auto cp0reg = r4300_cp0_regs(&g_dev.r4300.cp0);

                uint64_t data;
                const auto hex16str = sizeof("XXXXXXXXXXXXXXXX");
                char buf[hex16str];
                buf[hex16str - 1] = '\0';
                strncpy(buf, gdb_buffer + 2 + 32 * 16, sizeof(buf) - 1);
                sscanf(buf, "%" SCNx64, &data);
                cp0reg[CP0_STATUS_REG] = data;
                strncpy(buf, gdb_buffer + 2 + 33 * 16, sizeof(buf) - 1);
                sscanf(buf, "%" SCNx64, &data);
                g_dev.r4300.lo = data;
                strncpy(buf, gdb_buffer + 2 + 34 * 16, sizeof(buf) - 1);
                sscanf(buf, "%" SCNx64, &data);
                g_dev.r4300.hi = data;
                strncpy(buf, gdb_buffer + 2 + 35 * 16, sizeof(buf) - 1);
                sscanf(buf, "%" SCNx64, &data);
                cp0reg[CP0_BADVADDR_REG] = data;
                strncpy(buf, gdb_buffer + 2 + 36 * 16, sizeof(buf) - 1);
                sscanf(buf, "%" SCNx64, &data);
                cp0reg[CP0_CAUSE_REG] = data;
                strncpy(buf, gdb_buffer + 2 + 37 * 16, sizeof(buf) - 1);
                sscanf(buf, "%" SCNx64, &data);
                *r4300_pc(&g_dev.r4300) = data;

                strncpy(gdb_send_buffer, "$OK", sizeof("$OK") - 1);
                set_checksum_and_sendlength(sizeof("$OK") - 1);
            } else if(gdb_buffer[1] == '?') {
                //a ? is sent on connection by gdb
                if(g_dbg_runstate == M64P_DBG_RUNSTATE_PAUSED) {
                    auto& reply = "$S05";
                    strncpy(gdb_send_buffer, reply, sizeof(reply) - 1);
                    set_checksum_and_sendlength(sizeof(reply) - 1);
                }
                else g_dbg_runstate = M64P_DBG_RUNSTATE_PAUSED;
            } else if(gdb_buffer[1] == 's') {
                debugger_step();
            } else if(gdb_buffer[1] == 'c') {
                g_dbg_runstate = M64P_DBG_RUNSTATE_RUNNING;
                debugger_step();
            } else if(gdb_buffer[1] == 'Z' || gdb_buffer[1] == 'z') {
                unsigned int address;
                unsigned int kind; //for read/write, this is size bytes to watch at addr
                sscanf(gdb_buffer + 4, "%x,%x", &address, &kind);

                auto execbk = gdb_buffer[2] == '0' || gdb_buffer[2] == '1';
                auto writebk = gdb_buffer[2] == '2';
                auto readbk = gdb_buffer[2] == '3';

                unsigned int flags = M64P_BKP_FLAG_ENABLED | (
                    execbk ? M64P_BKP_FLAG_EXEC :
                    writebk ? M64P_BKP_FLAG_WRITE :
                    readbk ? M64P_BKP_FLAG_READ :
                    M64P_BKP_FLAG_WRITE | M64P_BKP_FLAG_READ);

                if(!execbk) address = DebugVirtualToPhysical(address);

                m64p_breakpoint bkpt = {
                    .address = address,
                    .endaddr = execbk ? address : address + kind,
                    .flags = flags
                };

                if(gdb_buffer[1] == 'Z') {
                    auto num = add_breakpoint_struct(&g_dev.mem, &bkpt);
                    if(num == -1) {
                        strncpy(gdb_send_buffer, "$E01", sizeof("$E01") - 1);
                        set_checksum_and_sendlength(sizeof("$E01") - 1);
                    } else {
                        enable_breakpoint(&g_dev.mem, num);
                        strncpy(gdb_send_buffer, "$OK", sizeof("$OK") - 1);
                        set_checksum_and_sendlength(sizeof("$OK") - 1);
                    }
                } else {
                    remove_breakpoint_by_address(&g_dev.mem, address);
                    strncpy(gdb_send_buffer, "$OK", sizeof("$OK") - 1);
                    set_checksum_and_sendlength(sizeof("$OK") - 1);
                }
            } else if(gdb_buffer[1] == 'm') {
                unsigned int address; 
                unsigned int length;
                sscanf(gdb_buffer + 2, "%x,%x", &address, &length);

                auto start = fast_mem_access_no_tlb_refill_exception(&g_dev.r4300, address);
                if(start == nullptr) {
                    strncpy(gdb_send_buffer, "$E01", sizeof("$E01") - 1);
                    set_checksum_and_sendlength(sizeof("$E01") - 1);
                } else {
                    gdb_send_buffer[0] = '$';

                    auto initialOffset = address & 0x3;
                    auto wordCount = ((initialOffset + length + 0x3) & ~0x3) >> 2;
                    for(unsigned int wordi = 0; wordi < wordCount; wordi++) {
                        auto word = start[wordi];
                        for(unsigned int bytei = wordi == 0 ? initialOffset : 0, writei = (unsigned int)0; wordi * 4 - initialOffset + bytei < length && bytei < 4; bytei++, writei++) {
                            auto byte = (word >> (3 - bytei) * 8) & 0xFF;
                            snprintf(gdb_send_buffer + 1 + wordi * 4 + writei * 2, 3, "%02x", byte);
                        }
                    }

                    set_checksum_and_sendlength(1 + length * 2);
                }
            } else if(gdb_buffer[1] == 'M') {
                unsigned int address;
                unsigned int length;
                sscanf(gdb_buffer + 2, "%x,%x", &address, &length);

                auto start = fast_mem_access_no_tlb_refill_exception(&g_dev.r4300, address);
                if(start == nullptr) {
                    strncpy(gdb_send_buffer, "$E01", sizeof("$E01") - 1);
                    set_checksum_and_sendlength(sizeof("$E01") - 1);
                } else {
                    auto initialOffset = address & 0x3;
                    auto wordCount = ((initialOffset + length + 0x3) & ~0x3) >> 2;
                    auto bytes = strchr(gdb_buffer, ':');
                    for(unsigned int wordi = 0; wordi < wordCount; wordi++) {
                        auto word = &start[wordi];
                        for(unsigned int bytei = wordi == 0 ? initialOffset : 0, writei = (unsigned int)0; wordi * 4 - initialOffset + bytei < length && bytei < 4; bytei++, writei++) {
                            unsigned int data;
                            char buf[3];
                            buf[2] = '\0';
                            strncpy(buf, bytes + 1 + wordi * 4 + writei * 2, sizeof(buf) - 1);
                            sscanf(buf, "%02x", &data);

                            auto mask = 0xFF << (3 - bytei) * 8;
                            *word &= ~mask;
                            *word |= data << (3 - bytei) * 8;
                        }
                    }

                    strncpy(gdb_send_buffer, "$OK", sizeof("$OK") - 1);
                    set_checksum_and_sendlength(sizeof("$OK") - 1);
                }
            } else {
                auto& emptyReply = "$#00";
                strncpy(gdb_send_buffer, emptyReply, sizeof(emptyReply) - 1);
                sendLength = sizeof(emptyReply) - 1;
            }

            SDLNet_TCP_Send(gdb_socket, gdb_send_buffer, sendLength);
        }
    }

    return 0;
}

void gdb_try_send_signal_stop() {
    if(gdb_socket == nullptr) return;
    gdb_send_signal_stop();
}

void set_checksum_and_sendlength(int hash_index) {
    gdb_send_buffer[hash_index] = '#';
    auto checkSend = std::accumulate(gdb_send_buffer + 1, gdb_send_buffer + hash_index, 0) % 256;
    snprintf(gdb_send_buffer + hash_index + 1, 3, "%02x", checkSend);
    sendLength = hash_index + sizeof("#XX") - 1;
}

void gdb_send_signal_stop()
{
    char stopb[7];
    auto& reply = "$S05#";
    strncpy(stopb, reply, sizeof(reply) - 1);
    auto checkSend = std::accumulate(reply + 1, reply + sizeof(reply) - 2, 0) % 256;
    snprintf(stopb + sizeof(reply) - 1, 3, "%02x", checkSend);
    auto sendLength = sizeof(reply) - 1 + 2;
    SDLNet_TCP_Send(gdb_socket, stopb, sendLength);
}

uint32_t DebugVirtualToPhysical(uint32_t address)
{
    struct device* dev = &g_dev;
    struct r4300_core* r4300 = &dev->r4300;

    if ((address & UINT32_C(0xc0000000)) != UINT32_C(0x80000000)) {
        address = virtual_to_physical_address_no_tlb_refill_exception(r4300, address, 0);
        if (address == 0) {
            return 0;
        }
    }

    address &= UINT32_C(0x1fffffff);
    return address;
}
