/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - arm_cpu_features.cpp                                    *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2015 Gilles Siberlin                                    *
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

#include <fstream>
#include <sstream>
#include <string>
#include "arm_cpu_features.h"

extern "C" {
    #include "api/callbacks.h"
}

arm_cpu_features_t arm_cpu_features;

const char procfile[] = "/proc/cpuinfo";

static unsigned char check_arm_cpu_feature(const std::string& feature)
{
    const std::string marker = "Features\t: ";

    std::string line;
    std::ifstream file(procfile);

    if (!file)
        return 0;

    while (std::getline(file, line))
    {
        if (line.find(marker) != std::string::npos)
        {
            std::stringstream line_stream(line);
            std::string token;

            while (std::getline(line_stream, token, ' '))
            {
                if (token == feature)
                    return 1;
            }
        }
    }
    return 0;
}

static unsigned char get_arm_cpu_implementer(void)
{
    const std::string marker = "CPU implementer\t: ";
    unsigned char implementer = 0;

    std::string line;
    std::ifstream file(procfile);

    if (!file)
        return 0;

    while (std::getline(file, line))
    {
        if (line.find(marker) != std::string::npos)
        {
            line = line.substr(marker.length());
            sscanf(line.c_str(), "0x%02hhx", &implementer);
            break;
        }
    }
    return implementer;
}

static unsigned short get_arm_cpu_part(void)
{
    const std::string marker = "CPU part\t: ";
    unsigned short part = 0;

    std::string line;
    std::ifstream file(procfile);

    if (!file)
        return 0;

    while (std::getline(file, line))
    {
        if (line.find(marker) != std::string::npos)
        {
            line = line.substr(marker.length());
            sscanf(line.c_str(), "0x%03hx", &part);
            break;
        }
    }
    return part;
}

void detect_arm_cpu_features(void)
{
    arm_cpu_features.SWP = check_arm_cpu_feature("swp");
    arm_cpu_features.Half = check_arm_cpu_feature("half");
    arm_cpu_features.Thumb = check_arm_cpu_feature("thumb");
    arm_cpu_features.FastMult = check_arm_cpu_feature("fastmult");
    arm_cpu_features.VFP = check_arm_cpu_feature("vfp");
    arm_cpu_features.EDSP = check_arm_cpu_feature("edsp");
    arm_cpu_features.ThumbEE = check_arm_cpu_feature("thumbee");
    arm_cpu_features.NEON = check_arm_cpu_feature("neon");
    arm_cpu_features.VFPv3 = check_arm_cpu_feature("vfpv3");
    arm_cpu_features.TLS = check_arm_cpu_feature("tls");
    arm_cpu_features.VFPv4 = check_arm_cpu_feature("vfpv4");
    arm_cpu_features.IDIVa = check_arm_cpu_feature("idiva");
    arm_cpu_features.IDIVt = check_arm_cpu_feature("idivt");
    // Qualcomm Krait supports IDIVa but it doesn't report it. Check for krait.
    if (get_arm_cpu_implementer() == 0x51 && get_arm_cpu_part() == 0x6F)
        arm_cpu_features.IDIVa = arm_cpu_features.IDIVt = 1;
}

void print_arm_cpu_features(void)
{
    std::string arm_cpu_features_string;

    arm_cpu_features_string="ARM CPU Features:";

    if (arm_cpu_features.SWP) arm_cpu_features_string += " SWP";
    if (arm_cpu_features.Half) arm_cpu_features_string += ", Half";
    if (arm_cpu_features.Thumb) arm_cpu_features_string += ", Thumb";
    if (arm_cpu_features.FastMult) arm_cpu_features_string += ", FastMult";
    if (arm_cpu_features.VFP) arm_cpu_features_string += ", VFP";
    if (arm_cpu_features.EDSP) arm_cpu_features_string += ", EDSP";
    if (arm_cpu_features.ThumbEE) arm_cpu_features_string += ", ThumbEE";
    if (arm_cpu_features.NEON) arm_cpu_features_string += ", NEON";
    if (arm_cpu_features.VFPv3) arm_cpu_features_string += ", VFPv3";
    if (arm_cpu_features.TLS) arm_cpu_features_string += ", TLS";
    if (arm_cpu_features.VFPv4) arm_cpu_features_string += ", VFPv4";
    if (arm_cpu_features.IDIVa) arm_cpu_features_string += ", IDIVa";
    if (arm_cpu_features.IDIVt) arm_cpu_features_string += ", IDIVt";

    DebugMessage(M64MSG_INFO, "%s", arm_cpu_features_string.c_str());
}

