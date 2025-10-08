/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - netplay.c                                               *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2020 loganmc10                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define SETTINGS_SIZE 24

#define M64P_CORE_PROTOTYPES 1
#include "api/callbacks.h"
#include "main.h"
#include "util.h"
#include "plugin/plugin.h"
#include "backends/plugins_compat/plugins_compat.h"
#include "netplay.h"
#include "osal/preproc.h"

#ifdef USE_SDL3NET
#include <SDL3_net/SDL_net.h>
#else
#include <SDL_net.h>
#endif

#if !defined(WIN32)
#include <netinet/ip.h>
#endif

#include <stdlib.h>

/* small helper structures to wrap SDL2_net/SDL3_net */

#ifdef USE_SDL3NET
/* SDL3_net needs a wrapper struct */
typedef struct
{
    uint8_t* data;
    int len;
    int maxlen;
} netplay_udp_packet;
#else
/* SDL2_net uses UDPpacket directly */
typedef UDPpacket netplay_udp_packet;
#endif

/* local variables */

#ifdef USE_SDL3NET
static NET_Address *l_resolvedAddress;
static int l_resolvedAddressPort;
static NET_StreamSocket *l_tcpSocket;
static NET_DatagramSocket *l_udpSocket;
#else
static UDPsocket l_udpSocket;
static TCPsocket l_tcpSocket;
#endif

static int l_canFF;
static int l_netplay_controller;
static int l_netplay_control[4];
static int l_udpChannel;
static int l_spectator;
static int l_netplay_is_init = 0;
static uint32_t l_vi_counter;
static uint8_t l_status;
static uint32_t l_reg_id;
static struct controller_input_compat *l_cin_compats;
static uint8_t l_plugin[4];
static uint8_t l_buffer_target;
static uint8_t l_player_lag[4];

//UDP packets
static netplay_udp_packet *l_request_input_packet;
static netplay_udp_packet *l_send_input_packet;
static netplay_udp_packet *l_process_packet;
static netplay_udp_packet *l_check_sync_packet;

static const int32_t l_check_sync_packet_size = (CP0_REGS_COUNT * 4) + 5;

//UDP packet formats
#define UDP_SEND_KEY_INFO 0
#define UDP_RECEIVE_KEY_INFO 1
#define UDP_REQUEST_KEY_INFO 2
#define UDP_RECEIVE_KEY_INFO_GRATUITOUS 3
#define UDP_SYNC_DATA 4

//TCP packet formats
#define TCP_SEND_SAVE 1
#define TCP_RECEIVE_SAVE 2
#define TCP_SEND_SETTINGS 3
#define TCP_RECEIVE_SETTINGS 4
#define TCP_REGISTER_PLAYER 5
#define TCP_GET_REGISTRATION 6
#define TCP_DISCONNECT_NOTICE 7

struct __UDPSocket {
    int ready;
    int channel;
};

#define CS4 32

/* small helper functions to wrap SDL2_net/SDL3_net functions */

#define netplay_min(x, y) (x > y ? y : x);

static netplay_udp_packet* alloc_udp_packet(size_t len)
{
#ifdef USE_SDL3NET
    netplay_udp_packet *packet = malloc(sizeof(netplay_udp_packet));
    if (packet == NULL)
        return NULL;

    packet->len = 0;
    packet->maxlen = len;
    packet->data = malloc(len);
    if (packet->data == NULL)
    {
        free(packet);
        return NULL;
    }
    return packet;
#else
    /* when using SDL2_net, netplay_udp_packet
     * is just a shim for UDPpacket */
    return SDLNet_AllocPacket(len);
#endif
}

static void free_udp_packet(netplay_udp_packet* packet)
{
    if (packet != NULL)
    {
#ifndef USE_SDL3NET
        SDLNet_FreePacket(packet);
#else
        if (packet->data != NULL)
        {
            free(packet->data);
        }
        free(packet);
#endif
    }
}

static osal_inline int netplay_recv_udp_packet(void* udpSocket, netplay_udp_packet* packet)
{
#ifdef USE_SDL3NET
    NET_Datagram* udpPacket = NULL;
    if (!NET_ReceiveDatagram((NET_DatagramSocket*)udpSocket, &udpPacket))
    {
        return 0;
    }

    if (udpPacket != NULL)
    {
        int len = netplay_min(packet->maxlen, udpPacket->buflen);
        memcpy(packet->data, udpPacket->buf, len);
        packet->len = len;
        NET_DestroyDatagram(udpPacket);
        return 1;
    }

    return 0;
#else
    return SDLNet_UDP_Recv((UDPsocket)udpSocket, packet);
#endif
}

static osal_inline void netplay_send_udp_packet(void* udpSocket, netplay_udp_packet* packet)
{
#ifdef USE_SDL3NET
    NET_SendDatagram((NET_DatagramSocket*)udpSocket, l_resolvedAddress, l_resolvedAddressPort, packet->data, packet->len);
#else
    SDLNet_UDP_Send((UDPsocket)udpSocket, l_udpChannel, packet);
#endif
}

static osal_inline int netplay_send_tcp_packet(void* tcpSocket, const void* data, int len)
{
#ifdef USE_SDL3NET
    NET_WriteToStreamSocket((NET_StreamSocket*)tcpSocket, data, len);
    NET_WaitUntilStreamSocketDrained((NET_StreamSocket*)tcpSocket, -1);
    return len;
#else
    return SDLNet_TCP_Send((TCPsocket)tcpSocket, data, len);
#endif
}

static osal_inline int netplay_recv_tcp_packet(void* tcpSocket, void* data, int len)
{
#ifdef USE_SDL3NET
    int ret;
    do
    {
        ret = NET_ReadFromStreamSocket((NET_StreamSocket*)tcpSocket, data, len);
    } while (ret == 0);
    return ret;
#else
    return SDLNet_TCP_Recv((TCPsocket)tcpSocket, data, len);
#endif   
}

static osal_inline void netplay_write32(uint32_t value, void *p)
{
#ifdef USE_SDL3NET
    *(uint32_t*)p = SDL_Swap32BE(value);
#else
    *(uint32_t*)p = SDL_SwapBE32(value);
#endif
}

static osal_inline uint32_t netplay_read32(void *p)
{
#ifdef USE_SDL3NET
    return SDL_Swap32BE(*(uint32_t*)p);
#else
    return SDL_SwapBE32(*(uint32_t*)p);
#endif
}

/* public exposed functions */

m64p_error netplay_start(const char* host, int port)
{
#ifdef USE_SDL3NET
    NET_Status status;

    if (!NET_Init())
    {
        DebugMessage(M64MSG_ERROR, "Netplay: Could not initialize SDL Net library: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    l_resolvedAddressPort = port;
    l_resolvedAddress = NET_ResolveHostname(host);
    if (l_resolvedAddress == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: Could not resolve host: %s", SDL_GetError());
        return M64ERR_SYSTEM_FAIL;
    }

    status = NET_WaitUntilResolved(l_resolvedAddress, -1);
    if (status != NET_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: Could not resolve host: %s", SDL_GetError());
        NET_UnrefAddress(l_resolvedAddress);
        l_resolvedAddress = NULL;
        return M64ERR_SYSTEM_FAIL;
    }

    l_udpSocket = NET_CreateDatagramSocket(NULL, 0);
    if (l_udpSocket == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: UDP socket creation failed: %s", SDL_GetError());
        NET_UnrefAddress(l_resolvedAddress);
        l_resolvedAddress = NULL;
        return M64ERR_SYSTEM_FAIL;
    }

    l_tcpSocket = NET_CreateClient(l_resolvedAddress, port);
    if (l_tcpSocket == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: TCP socket connection failed: %s", SDL_GetError());
        NET_UnrefAddress(l_resolvedAddress);
        NET_DestroyDatagramSocket(l_udpSocket);
        l_resolvedAddress = NULL;
        l_udpSocket = NULL;
        return M64ERR_SYSTEM_FAIL;
    }

    status = NET_WaitUntilConnected(l_tcpSocket, 120 * 1000);
    if (status != NET_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: TCP socket connection failed: %s", SDL_GetError());
        NET_UnrefAddress(l_resolvedAddress);
        NET_DestroyDatagramSocket(l_udpSocket);
        NET_DestroyStreamSocket(l_tcpSocket);
        l_resolvedAddress = NULL;
        l_udpSocket = NULL;
        l_tcpSocket = NULL;
        return M64ERR_SYSTEM_FAIL;
    }

    // dummy value
    l_udpChannel = 1;
#else
    if (SDLNet_Init() < 0)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: Could not initialize SDL Net library");
        return M64ERR_SYSTEM_FAIL;
    }

    l_udpSocket = SDLNet_UDP_Open(0);
    if (l_udpSocket == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: UDP socket creation failed");
        return M64ERR_SYSTEM_FAIL;
    }

#if !defined(WIN32)
    const char tos_local = CS4 << 2;
    struct __UDPSocket* socket = (struct __UDPSocket*) l_udpSocket;
    setsockopt(socket->channel, IPPROTO_IP, IP_TOS, &tos_local, sizeof(tos_local));
#endif

    IPaddress dest;
    SDLNet_ResolveHost(&dest, host, port);

    l_udpChannel = SDLNet_UDP_Bind(l_udpSocket, -1, &dest);
    if (l_udpChannel < 0)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: could not bind to UDP socket");
        SDLNet_UDP_Close(l_udpSocket);
        l_udpSocket = NULL;
        return M64ERR_SYSTEM_FAIL;
    }

    l_tcpSocket = SDLNet_TCP_Open(&dest);
    if (l_tcpSocket == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: could not bind to TCP socket");
        SDLNet_UDP_Close(l_udpSocket);
        l_udpSocket = NULL;
        return M64ERR_SYSTEM_FAIL;
    }
#endif

    l_request_input_packet = alloc_udp_packet(12);
    l_send_input_packet = alloc_udp_packet(11);
    l_process_packet = alloc_udp_packet(512);
    l_check_sync_packet = alloc_udp_packet(l_check_sync_packet_size);
    if (l_request_input_packet == NULL ||
        l_send_input_packet == NULL ||
        l_process_packet == NULL ||
        l_check_sync_packet == NULL)
    {
        DebugMessage(M64MSG_ERROR, "Netplay: could not allocate UDP packets");
#ifdef USE_SDL3NET
        NET_UnrefAddress(l_resolvedAddress);
        l_resolvedAddress = NULL;

        NET_DestroyDatagramSocket(l_udpSocket);
        NET_DestroyStreamSocket(l_tcpSocket);
#else
        SDLNet_UDP_Close(l_udpSocket);
        SDLNet_TCP_Close(l_tcpSocket);
#endif
        l_udpSocket = NULL;
        l_tcpSocket = NULL;
        free_udp_packet(l_request_input_packet);
        l_request_input_packet = NULL;
        free_udp_packet(l_send_input_packet);
        l_send_input_packet = NULL;
        free_udp_packet(l_process_packet);
        l_process_packet = NULL;
        free_udp_packet(l_check_sync_packet);
        l_check_sync_packet = NULL;
        return M64ERR_NO_MEMORY;
    }

    for (int i = 0; i < 4; ++i)
    {
        l_netplay_control[i] = -1;
        l_plugin[i] = 0;
        l_player_lag[i] = 0;
    }

    l_canFF = 0;
    l_netplay_controller = 0;
    l_netplay_is_init = 1;
    l_spectator = 1;
    l_vi_counter = 0;
    l_status = 0;
    l_reg_id = 0;

    return M64ERR_SUCCESS;
}

m64p_error netplay_stop()
{
    if (l_udpSocket == NULL)
        return M64ERR_INVALID_STATE;
    else
    {
        if (l_cin_compats != NULL)
        {
            for (int i = 0; i < 4; ++i)
            {
                struct netplay_event* current = l_cin_compats[i].event_first;
                struct netplay_event* next;
                while (current != NULL)
                {
                    next = current->next;
                    free(current);
                    current = next;
                }
            }
        }

        char output_data[5];
        output_data[0] = TCP_DISCONNECT_NOTICE;
        netplay_write32(l_reg_id, &output_data[1]);
        netplay_send_tcp_packet(l_tcpSocket, &output_data[0], 5);

#ifdef USE_SDL3NET
        NET_UnrefAddress(l_resolvedAddress);
        l_resolvedAddress = NULL;

        NET_DestroyDatagramSocket(l_udpSocket);
        NET_DestroyStreamSocket(l_tcpSocket);
#else
        SDLNet_UDP_Unbind(l_udpSocket, l_udpChannel);
        SDLNet_UDP_Close(l_udpSocket);
        SDLNet_TCP_Close(l_tcpSocket);
#endif
        l_tcpSocket = NULL;
        l_udpSocket = NULL;
        l_udpChannel = -1;

        free_udp_packet(l_request_input_packet);
        free_udp_packet(l_send_input_packet);
        free_udp_packet(l_process_packet);
        free_udp_packet(l_check_sync_packet);
        l_request_input_packet = NULL;
        l_send_input_packet = NULL;
        l_process_packet = NULL;
        l_check_sync_packet = NULL;
    
        l_netplay_is_init = 0;

#ifdef USE_SDL3NET
        NET_Quit();
#else
        SDLNet_Quit();
#endif
        return M64ERR_SUCCESS;
    }
}

int netplay_is_init()
{
    return l_netplay_is_init;
}

static uint8_t buffer_size(uint8_t control_id)
{
    //This function returns the size of the local input buffer
    uint8_t counter = 0;
    struct netplay_event* current = l_cin_compats[control_id].event_first;
    while (current != NULL)
    {
        current = current->next;
        ++counter;
    }
    return counter;
}

static void netplay_request_input(uint8_t control_id)
{
    l_request_input_packet->data[0] = UDP_REQUEST_KEY_INFO;
    l_request_input_packet->data[1] = control_id; //The player we need input for
    netplay_write32(l_reg_id, &l_request_input_packet->data[2]); //our registration ID
    netplay_write32(l_cin_compats[control_id].netplay_count, &l_request_input_packet->data[6]); //the current event count
    l_request_input_packet->data[10] = l_spectator; //whether we are a spectator
    l_request_input_packet->data[11] = buffer_size(control_id); //our local buffer size
    l_request_input_packet->len = 12;
    netplay_send_udp_packet(l_udpSocket, l_request_input_packet);
}

static int check_valid(uint8_t control_id, uint32_t count)
{
    //Check if we already have this event recorded locally, returns 1 if we do
    struct netplay_event* current = l_cin_compats[control_id].event_first;
    while (current != NULL)
    {
        if (current->count == count) //event already recorded
            return 1;
        current = current->next;
    }
    return 0;
}

static int netplay_require_response(void* opaque)
{
    //This function runs inside a thread.
    //It runs if our local buffer size is 0 (we need to execute a key event, but we don't have the data we need).
    //We basically beg the server for input data.
    //After 10 seconds a timeout occurs, we assume we have lost connection to the server.
    uint8_t control_id = *(uint8_t*)opaque;
    uint64_t timeout = SDL_GetTicks() + 10000;
    while (!check_valid(control_id, l_cin_compats[control_id].netplay_count))
    {
        if (SDL_GetTicks() > timeout)
        {
            l_udpChannel = -1;
            return 0;
        }
        netplay_request_input(control_id);
        SDL_Delay(5);
    }
    return 1;
}

static void netplay_process()
{
    //In this function we process data we have received from the server
    uint32_t curr, count, keys;
    uint8_t plugin, player, current_status;
    while (netplay_recv_udp_packet(l_udpSocket, l_process_packet) == 1)
    {
        switch (l_process_packet->data[0])
        {
            case UDP_RECEIVE_KEY_INFO:
            case UDP_RECEIVE_KEY_INFO_GRATUITOUS:
                player = l_process_packet->data[1];
                //current_status is a status update from the server
                //it will let us know if another player has disconnected, or the games have desynced
                current_status = l_process_packet->data[2];
                if (l_process_packet->data[0] == UDP_RECEIVE_KEY_INFO)
                    l_player_lag[player] = l_process_packet->data[3];
                if (current_status != l_status)
                {
                    if (((current_status & 0x1) ^ (l_status & 0x1)) != 0)
                        DebugMessage(M64MSG_ERROR, "Netplay: players have de-synced at VI %u", l_vi_counter);
                    for (int dis = 1; dis < 5; ++dis)
                    {
                        if (((current_status & (0x1 << dis)) ^ (l_status & (0x1 << dis))) != 0)
                            DebugMessage(M64MSG_ERROR, "Netplay: player %u has disconnected", dis);
                    }
                    l_status = current_status;
                }
                curr = 5;
                //this loop processes input data from the server, inserting new events into the linked list for each player
                //it skips events that we have already recorded, or if we receive data for an event that has already happened
                for (uint8_t i = 0; i < l_process_packet->data[4]; ++i)
                {
                    count = netplay_read32(&l_process_packet->data[curr]);
                    curr += 4;

                    if (((count - l_cin_compats[player].netplay_count) > (UINT32_MAX / 2)) || (check_valid(player, count))) //event doesn't need to be recorded
                    {
                        curr += 5;
                        continue;
                    }

                    keys = netplay_read32(&l_process_packet->data[curr]);
                    curr += 4;
                    plugin = l_process_packet->data[curr];
                    curr += 1;

                    //insert new event at beginning of linked list
                    struct netplay_event* new_event = (struct netplay_event*)malloc(sizeof(struct netplay_event));
                    new_event->count = count;
                    new_event->buttons = keys;
                    new_event->plugin = plugin;
                    new_event->next = l_cin_compats[player].event_first;
                    l_cin_compats[player].event_first = new_event;
                }
                break;
            default:
                DebugMessage(M64MSG_ERROR, "Netplay: received unknown message from server");
                break;
        }
    }
}

static int netplay_ensure_valid(uint8_t control_id)
{
    //This function makes sure we have data for a certain event
    //If we don't have the data, it will create a new thread that will request the data
    if (check_valid(control_id, l_cin_compats[control_id].netplay_count))
        return 1;

    if (l_udpChannel == -1)
        return 0;

    SDL_Thread* thread = SDL_CreateThread(netplay_require_response, "Netplay key request", &control_id);

    while (!check_valid(control_id, l_cin_compats[control_id].netplay_count) && l_udpChannel != -1)
        netplay_process();
    int success;
    SDL_WaitThread(thread, &success);
    return success;
}

static void netplay_delete_event(struct netplay_event* current, uint8_t control_id)
{
    //This function deletes an event from the linked list
    struct netplay_event* find = l_cin_compats[control_id].event_first;
    while (find != NULL)
    {
        if (find->next == current)
        {
            find->next = current->next;
            break;
        }
        find = find->next;
    }
    if (current == l_cin_compats[control_id].event_first)
        l_cin_compats[control_id].event_first = l_cin_compats[control_id].event_first->next;
    free(current);
}

static uint32_t netplay_get_input(uint8_t control_id)
{
    uint32_t keys;
    netplay_process();
    netplay_request_input(control_id);

    //l_buffer_target is set by the server upon registration
    //l_player_lag is how far behind we are from the lead player
    //buffer_size is the local buffer size
    if (l_player_lag[control_id] > 0 && buffer_size(control_id) > l_buffer_target)
    {
        l_canFF = 1;
        main_core_state_set(M64CORE_SPEED_LIMITER, 0);
    }
    else
    {
        main_core_state_set(M64CORE_SPEED_LIMITER, 1);
        l_canFF = 0;
    }

    if (netplay_ensure_valid(control_id))
    {
        //We grab the event from the linked list, the delete it once it has been used
        //Finally we increment the event counter
        struct netplay_event* current = l_cin_compats[control_id].event_first;
        while (current->count != l_cin_compats[control_id].netplay_count)
            current = current->next;
        keys = current->buttons;
        Controls[control_id].Plugin = current->plugin;
        netplay_delete_event(current, control_id);
        ++l_cin_compats[control_id].netplay_count;
    }
    else
    {
        DebugMessage(M64MSG_ERROR, "Netplay: lost connection to server");
        main_core_state_set(M64CORE_EMU_STATE, M64EMU_STOPPED);
        keys = 0;
    }

    return keys;
}

static void netplay_send_input(uint8_t control_id, uint32_t keys)
{
    l_send_input_packet->data[0] = UDP_SEND_KEY_INFO;
    l_send_input_packet->data[1] = control_id; //player number
    netplay_write32(l_cin_compats[control_id].netplay_count, &l_send_input_packet->data[2]); // current event count
    netplay_write32(keys, &l_send_input_packet->data[6]); //key data
    l_send_input_packet->data[10] = l_plugin[control_id]; //current plugin
    l_send_input_packet->len = 11;
    netplay_send_udp_packet(l_udpSocket, l_send_input_packet);
}

uint8_t netplay_register_player(uint8_t player, uint8_t plugin, uint8_t rawdata, uint32_t reg_id)
{
    l_reg_id = reg_id;
    char output_data[8];
    output_data[0] = TCP_REGISTER_PLAYER;
    output_data[1] = player; //player number we'd like to register
    output_data[2] = plugin; //current plugin
    output_data[3] = rawdata; //whether we are using a RawData input plugin
    netplay_write32(l_reg_id, &output_data[4]);

    netplay_send_tcp_packet(l_tcpSocket, &output_data[0], 8);

    uint8_t response[2];
    size_t recv = 0;
    while (recv < 2)
        recv += netplay_recv_tcp_packet(l_tcpSocket, &response[recv], 2 - recv);
    l_buffer_target = response[1]; //local buffer size target
    return response[0];
}

int netplay_lag()
{
    return l_canFF;
}

int netplay_next_controller()
{
    return l_netplay_controller;
}

void netplay_set_controller(uint8_t player)
{
    l_netplay_control[player] = l_netplay_controller++;
    l_spectator = 0;
}

int netplay_get_controller(uint8_t player)
{
    return l_netplay_control[player];
}

file_status_t netplay_read_storage(const char *filename, void *data, size_t size)
{
    //This function syncs save games.
    //If the client is controlling player 1, it sends its save game to the server
    //All other players receive save files from the server
    const char *file_extension = strrchr(filename, '.');
    file_extension += 1;

    uint32_t buffer_pos = 0;
    char *output_data = malloc(size + strlen(file_extension) + 6);

    file_status_t ret;
    uint8_t request;
    if (l_netplay_control[0] != -1)
    {
        request = TCP_SEND_SAVE;
        memcpy(&output_data[buffer_pos], &request, 1);
        ++buffer_pos;

         //send file extension
        memcpy(&output_data[buffer_pos], file_extension, strlen(file_extension) + 1);
        buffer_pos += strlen(file_extension) + 1;

        ret = read_from_file(filename, data, size);
        if (ret == file_open_error)
            memset(data, 0, size); //all zeros means there is no save file
        netplay_write32((int32_t)size, &output_data[buffer_pos]); //file data size
        buffer_pos += 4;
        memcpy(&output_data[buffer_pos], data, size); //file data
        buffer_pos += size;

        netplay_send_tcp_packet(l_tcpSocket, &output_data[0], buffer_pos);
    }
    else
    {
        request = TCP_RECEIVE_SAVE;
        memcpy(&output_data[buffer_pos], &request, 1);
        ++buffer_pos;

        //extension of the file we are requesting
        memcpy(&output_data[buffer_pos], file_extension, strlen(file_extension) + 1);
        buffer_pos += strlen(file_extension) + 1;

        netplay_send_tcp_packet(l_tcpSocket, &output_data[0], buffer_pos);
        size_t recv = 0;
        char *data_array = data;
        while (recv < size)
            recv += netplay_recv_tcp_packet(l_tcpSocket, data_array + recv, size - recv);

        int sum = 0;
        for (int i = 0; i < size; ++i)
            sum |= data_array[i];

        if (sum == 0) //all zeros means there is no save file
            ret = file_open_error;
        else
            ret = file_ok;
    }
    free(output_data);
    return ret;
}

void netplay_sync_settings(uint32_t *count_per_op, uint32_t *count_per_op_denom_pot, uint32_t *disable_extra_mem, int32_t *si_dma_duration, uint32_t *emumode, int32_t *no_compiled_jump)
{
    if (!netplay_is_init())
        return;

    char output_data[SETTINGS_SIZE + 1];
    uint8_t request;
    if (l_netplay_control[0] != -1) //player 1 is the source of truth for settings
    {
        request = TCP_SEND_SETTINGS;
        memcpy(&output_data[0], &request, 1);
        netplay_write32(*count_per_op, &output_data[1]);
        netplay_write32(*count_per_op_denom_pot, &output_data[5]);
        netplay_write32(*disable_extra_mem, &output_data[9]);
        netplay_write32(*si_dma_duration, &output_data[13]);
        netplay_write32(*emumode, &output_data[17]);
        netplay_write32(*no_compiled_jump, &output_data[21]);
        netplay_send_tcp_packet(l_tcpSocket, &output_data[0], SETTINGS_SIZE + 1);
    }
    else
    {
        request = TCP_RECEIVE_SETTINGS;
        memcpy(&output_data[0], &request, 1);
        netplay_send_tcp_packet(l_tcpSocket, &output_data[0], 1);
        int32_t recv = 0;
        while (recv < SETTINGS_SIZE)
            recv += netplay_recv_tcp_packet(l_tcpSocket, &output_data[recv], SETTINGS_SIZE - recv);
        *count_per_op = netplay_read32(&output_data[0]);
        *count_per_op_denom_pot = netplay_read32(&output_data[4]);
        *disable_extra_mem = netplay_read32(&output_data[8]);
        *si_dma_duration = netplay_read32(&output_data[12]);
        *emumode = netplay_read32(&output_data[16]);
        *no_compiled_jump = netplay_read32(&output_data[20]);
    }
}

void netplay_check_sync(struct cp0* cp0)
{
    //This function is used to check if games have desynced
    //Every 600 VIs, it sends the value of the CP0 registers to the server
    //The server will compare the values, and update the status byte if it detects a desync
    if (!netplay_is_init())
        return;

    if (l_vi_counter % 600 == 0)
    {
        const uint32_t* cp0_regs = r4300_cp0_regs(cp0);

        l_check_sync_packet->data[0] = UDP_SYNC_DATA;
        netplay_write32(l_vi_counter, &l_check_sync_packet->data[1]); //current VI count
        for (int i = 0; i < CP0_REGS_COUNT; ++i)
        {
            netplay_write32(cp0_regs[i], &l_check_sync_packet->data[(i * 4) + 5]);
        }
        l_check_sync_packet->len = l_check_sync_packet_size;
        netplay_send_udp_packet(l_udpSocket, l_check_sync_packet);
    }

    ++l_vi_counter;
}

void netplay_read_registration(struct controller_input_compat* cin_compats)
{
    //This function runs right before the game starts
    //The server shares the registration details about each player
    if (!netplay_is_init())
        return;

    l_cin_compats = cin_compats;

    uint32_t reg_id;
    char output_data = TCP_GET_REGISTRATION;
    char input_data[24];
    netplay_send_tcp_packet(l_tcpSocket, &output_data, 1);
    size_t recv = 0;
    while (recv < 24)
        recv += netplay_recv_tcp_packet(l_tcpSocket, &input_data[recv], 24 - recv);
    uint32_t curr = 0;
    for (int i = 0; i < 4; ++i)
    {
        reg_id = netplay_read32(&input_data[curr]);
        curr += 4;

        Controls[i].Type = CONT_TYPE_STANDARD; //make sure VRU is disabled

        if (reg_id == 0) //No one registered to control this player
        {
            Controls[i].Present = 0;
            Controls[i].Plugin = PLUGIN_NONE;
            Controls[i].RawData = 0;
            curr += 2;
        }
        else
        {
            Controls[i].Present = 1;
            if (i > 0 && input_data[curr] == PLUGIN_MEMPAK) // only P1 can use mempak
                Controls[i].Plugin = PLUGIN_NONE;
            else if (input_data[curr] == PLUGIN_TRANSFER_PAK) // Transferpak not supported during netplay
                Controls[i].Plugin = PLUGIN_NONE;
            else
                Controls[i].Plugin = input_data[curr];
            l_plugin[i] = Controls[i].Plugin;
            ++curr;
            Controls[i].RawData = input_data[curr];
            ++curr;
        }
    }
}

static void netplay_send_raw_input(struct pif* pif)
{
    for (int i = 0; i < 4; ++i)
    {
        if (l_netplay_control[i] != -1)
        {
            if (pif->channels[i].tx && pif->channels[i].tx_buf[0] == JCMD_CONTROLLER_READ)
                netplay_send_input(i, *(uint32_t*)pif->channels[i].rx_buf);
        }
    }
}

static void netplay_get_raw_input(struct pif* pif)
{
    for (int i = 0; i < 4; ++i)
    {
        if (Controls[i].Present == 1)
        {
            if (pif->channels[i].tx)
            {
                *pif->channels[i].rx &= ~0xC0; //Always show the controller as connected

                if(pif->channels[i].tx_buf[0] == JCMD_CONTROLLER_READ)
                {
                    *(uint32_t*)pif->channels[i].rx_buf = netplay_get_input(i);
                }
                else if ((pif->channels[i].tx_buf[0] == JCMD_STATUS || pif->channels[i].tx_buf[0] == JCMD_RESET) && Controls[i].RawData)
                {
                    //a bit of a hack for raw input controllers, force the status
                    uint16_t type = JDT_JOY_ABS_COUNTERS | JDT_JOY_PORT;
                    pif->channels[i].rx_buf[0] = (uint8_t)(type >> 0);
                    pif->channels[i].rx_buf[1] = (uint8_t)(type >> 8);
                    pif->channels[i].rx_buf[2] = 0;
                }
                else if (pif->channels[i].tx_buf[0] == JCMD_PAK_READ && Controls[i].RawData)
                {
                    //also a hack for raw input, we return "mempak not present" if the game tries to read the mempak
                    pif->channels[i].rx_buf[32] = 255;
                }
                else if (pif->channels[i].tx_buf[0] == JCMD_PAK_WRITE && Controls[i].RawData)
                {
                    //also a hack for raw input, we return "mempak not present" if the game tries to write to mempak
                    pif->channels[i].rx_buf[0] = 255;
                }
            }
        }
    }
}

void netplay_update_input(struct pif* pif)
{
    if (netplay_is_init())
    {
        netplay_send_raw_input(pif);
        netplay_get_raw_input(pif);
    }
}

m64p_error netplay_send_config(char* data, int size)
{
    if (!netplay_is_init())
        return M64ERR_NOT_INIT;

    if (l_netplay_control[0] != -1 || size == 1) //Only P1 sends settings, we allow all players to send if the size is 1, this may be a request packet
    {
        int result = netplay_send_tcp_packet(l_tcpSocket, data, size);
        if (result < size)
            return M64ERR_SYSTEM_FAIL;
        return M64ERR_SUCCESS;
    }
    else
        return M64ERR_INVALID_STATE;
}

m64p_error netplay_receive_config(char* data, int size)
{
    if (!netplay_is_init())
        return M64ERR_NOT_INIT;

    if (l_netplay_control[0] == -1) //Only P2-4 receive settings
    {
        int recv = 0;
        while (recv < size)
        {
            recv += netplay_recv_tcp_packet(l_tcpSocket, &data[recv], size - recv);
            if (recv < 1)
                return M64ERR_SYSTEM_FAIL;
        }
        return M64ERR_SUCCESS;
    }
    else
        return M64ERR_INVALID_STATE;
}
