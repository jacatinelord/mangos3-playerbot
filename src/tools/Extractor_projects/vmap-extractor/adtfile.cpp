/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include <algorithm>
#include <cstdio>
#include "vmapexport.h"
#include "adtfile.h"

#ifdef WIN32
#define snprintf _snprintf
#endif

const char* GetPlainName(const char* FileName)
{
    const char* szTemp;

    if ((szTemp = strrchr(FileName, '\\')) != NULL)
    {
        FileName = szTemp + 1;
    }
    return FileName;
}

char* GetPlainName(char* FileName)
{
    char* szTemp;

    if ((szTemp = strrchr(FileName, '\\')) != NULL)
    {
        FileName = szTemp + 1;
    }
    return FileName;
}

void fixnamen(char* name, size_t len)
{
    for (size_t i = 0; i < len - 3; i++)
    {
        if (i > 0 && name[i] >= 'A' && name[i] <= 'Z' && isalpha(name[i - 1]))
        {
            name[i] |= 0x20;
        }
        else if ((i == 0 || !isalpha(name[i - 1])) && name[i] >= 'a' && name[i] <= 'z')
        {
            name[i] &= ~0x20;
        }
    }
    //extension in lowercase
    for (size_t i = len - 3; i < len; i++)
    {
        name[i] |= 0x20;
    }
}

void fixname2(char* name, size_t len)
{
    for (size_t i = 0; i < len - 3; i++)
    {
        if (name[i] == ' ')
        {
            name[i] = '_';
        }
    }
}

char const* GetExtension(char const* FileName)
{
    char const* szTemp;
    if ((szTemp = strrchr(FileName, '.')) != NULL)
    {
        return szTemp;
    }
    return NULL;
}

extern HANDLE WorldMpq;

ADTFile::ADTFile(char* filename): ADT(WorldMpq, filename)
{
    AdtFilename.append(filename);
}

bool ADTFile::init(uint32 map_num, uint32 tileX, uint32 tileY, StringSet& failedPaths)
{
    if (ADT.isEof())
    {
        return false;
    }

    uint32 size;

    std::string xMap;
    std::string yMap;

    AdtFilename.erase(AdtFilename.find(".adt"), 4);
    std::string TempMapNumber;

    TempMapNumber = AdtFilename.substr(AdtFilename.length() - 6, 6);
    xMap = TempMapNumber.substr(TempMapNumber.find("_") + 1, (TempMapNumber.find_last_of("_") - 1) - (TempMapNumber.find("_")));
    yMap = TempMapNumber.substr(TempMapNumber.find_last_of("_") + 1, (TempMapNumber.length()) - (TempMapNumber.find_last_of("_")));
    AdtFilename.erase((AdtFilename.length() - xMap.length() - yMap.length() - 2), (xMap.length() + yMap.length() + 2));

    string AdtMapNumber = xMap + ' ' + yMap + ' ' + GetPlainName((char*)AdtFilename.c_str());

    std::string dirname = std::string(szWorkDirWmo) + "/dir_bin";
    FILE* dirfile;
    dirfile = fopen(dirname.c_str(), "ab");
    if (!dirfile)
    {
        printf("Can't open dirfile!'%s'\n", dirname.c_str());
        return false;
    }

    while (!ADT.isEof())
    {
        char fourcc[5];
        ADT.read(&fourcc, 4);
        ADT.read(&size, 4);
        flipcc(fourcc);
        fourcc[4] = 0;

        size_t nextpos = ADT.getPos() + size;

        if (!strcmp(fourcc, "MCIN"))
        {
        }
        else if (!strcmp(fourcc, "MTEX"))
        {
        }
        else if (!strcmp(fourcc, "MMDX"))
        {
            if (size)
            {
                char* buf = new char[size];
                ADT.read(buf, size);
                char* p = buf;
                int t = 0;
                ModelInstansName = new std::string[size];
                while (p < buf + size)
                {
                    fixnamen(p, strlen(p));
                    char* s = GetPlainName(p);
                    fixname2(s, strlen(s));
                    std::string path(p);                         // Store copy after name fixed
                    std::string uName;
                    ExtractSingleModel(path, uName, failedPaths);
                    ModelInstansName[t++] = uName;
                    p = p + strlen(p) + 1;
                }
                delete[] buf;
            }
        }
        else if (!strcmp(fourcc, "MWMO"))
        {
            if (size)
            {
                char* buf = new char[size];
                ADT.read(buf, size);
                char* p = buf;
                int q = 0;
                WmoInstansName = new std::string[size];
                while (p < buf + size)
                {
                    std::string path(p);
                    char* s = GetPlainName(p);
                    fixnamen(s, strlen(s));
                    fixname2(s, strlen(s));
                    p = p + strlen(p) + 1;
                    WmoInstansName[q++] = s;
                }
                delete[] buf;
            }
        }
        //======================
        else if (!strcmp(fourcc, "MDDF"))
        {
            if (size)
            {
                nMDX = (int)size / 36;
                for (int i = 0; i < nMDX; ++i)
                {
                    uint32 id;
                    ADT.read(&id, 4);
                    ModelInstance inst(ADT, ModelInstansName[id].c_str(), map_num, tileX, tileY, dirfile);
                }
                delete[] ModelInstansName;
            }
        }
        else if (!strcmp(fourcc, "MODF"))
        {
            if (size)
            {
                nWMO = (int)size / 64;
                for (int i = 0; i < nWMO; ++i)
                {
                    uint32 id;
                    ADT.read(&id, 4);
                    WMOInstance inst(ADT, WmoInstansName[id].c_str(), map_num, tileX, tileY, dirfile);
                }
                delete[] WmoInstansName;
            }
        }
        //======================
        ADT.seek(nextpos);
    }
    ADT.close();
    fclose(dirfile);

    return true;
}

ADTFile::~ADTFile()
{
    ADT.close();
}
