/*
 *  skl-topology.c - Implements Platform component ALSA controls/widget
 *  handlers.
 *
 *  Copyright (C) 2014-2015 Intel Corp
 *  Author: Jeeja KP <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/firmware.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/soc-topology.h>
#include "skl-sst-dsp.h"
#include "skl-sst-ipc.h"
#include "skl-topology.h"
#include "skl.h"
#include "skl-tplg-interface.h"
#include "skl-fwlog.h"

#define SKL_CH_FIXUP_MASK		(1 << 0)
#define SKL_RATE_FIXUP_MASK		(1 << 1)
#define SKL_FMT_FIXUP_MASK		(1 << 2)

/*
 * The following table provides the gain in linear scale corresponding to
 * gain in dB scale in the range of -144 dB to 0 dB with 0.1 dB resolution.
 * The real number linear gain is scaled by 0x7FFFFFFFF to convert it to a
 * 32 bit integer as required by FW.
 * linear_gain[i] = 0   for i = 0 ; (Mapped as mute)
 *		  = 0x7FFFFFFF*Round(10^(-144+0.1*i)/20) for i = 1 ... 1440
 */
static u32 linear_gain[] = {
0x00000000, 0x00000089, 0x0000008B, 0x0000008C, 0x0000008E, 0x00000090, 0x00000091, 0x00000093,
0x00000095, 0x00000096, 0x00000098, 0x0000009A, 0x0000009C, 0x0000009D, 0x0000009F, 0x000000A1,
0x000000A3, 0x000000A5, 0x000000A7, 0x000000A9, 0x000000AB, 0x000000AD, 0x000000AF, 0x000000B1,
0x000000B3, 0x000000B5, 0x000000B7, 0x000000B9, 0x000000BB, 0x000000BD, 0x000000BF, 0x000000C2,
0x000000C4, 0x000000C6, 0x000000C8, 0x000000CB, 0x000000CD, 0x000000CF, 0x000000D2, 0x000000D4,
0x000000D7, 0x000000D9, 0x000000DC, 0x000000DE, 0x000000E1, 0x000000E3, 0x000000E6, 0x000000E9,
0x000000EB, 0x000000EE, 0x000000F1, 0x000000F4, 0x000000F7, 0x000000F9, 0x000000FC, 0x000000FF,
0x00000102, 0x00000105, 0x00000108, 0x0000010B, 0x0000010E, 0x00000111, 0x00000115, 0x00000118,
0x0000011B, 0x0000011E, 0x00000122, 0x00000125, 0x00000128, 0x0000012C, 0x0000012F, 0x00000133,
0x00000136, 0x0000013A, 0x0000013E, 0x00000141, 0x00000145, 0x00000149, 0x0000014D, 0x00000150,
0x00000154, 0x00000158, 0x0000015C, 0x00000160, 0x00000164, 0x00000169, 0x0000016D, 0x00000171,
0x00000175, 0x0000017A, 0x0000017E, 0x00000182, 0x00000187, 0x0000018B, 0x00000190, 0x00000195,
0x00000199, 0x0000019E, 0x000001A3, 0x000001A8, 0x000001AC, 0x000001B1, 0x000001B6, 0x000001BC,
0x000001C1, 0x000001C6, 0x000001CB, 0x000001D0, 0x000001D6, 0x000001DB, 0x000001E1, 0x000001E6,
0x000001EC, 0x000001F2, 0x000001F7, 0x000001FD, 0x00000203, 0x00000209, 0x0000020F, 0x00000215,
0x0000021B, 0x00000222, 0x00000228, 0x0000022E, 0x00000235, 0x0000023B, 0x00000242, 0x00000249,
0x0000024F, 0x00000256, 0x0000025D, 0x00000264, 0x0000026B, 0x00000273, 0x0000027A, 0x00000281,
0x00000289, 0x00000290, 0x00000298, 0x0000029F, 0x000002A7, 0x000002AF, 0x000002B7, 0x000002BF,
0x000002C7, 0x000002CF, 0x000002D8, 0x000002E0, 0x000002E9, 0x000002F1, 0x000002FA, 0x00000303,
0x0000030C, 0x00000315, 0x0000031E, 0x00000327, 0x00000330, 0x0000033A, 0x00000343, 0x0000034D,
0x00000357, 0x00000361, 0x0000036B, 0x00000375, 0x0000037F, 0x0000038A, 0x00000394, 0x0000039F,
0x000003A9, 0x000003B4, 0x000003BF, 0x000003CA, 0x000003D6, 0x000003E1, 0x000003EC, 0x000003F8,
0x00000404, 0x00000410, 0x0000041C, 0x00000428, 0x00000434, 0x00000441, 0x0000044D, 0x0000045A,
0x00000467, 0x00000474, 0x00000481, 0x0000048F, 0x0000049C, 0x000004AA, 0x000004B8, 0x000004C6,
0x000004D4, 0x000004E2, 0x000004F1, 0x000004FF, 0x0000050E, 0x0000051D, 0x0000052C, 0x0000053B,
0x0000054B, 0x0000055B, 0x0000056B, 0x0000057B, 0x0000058B, 0x0000059B, 0x000005AC, 0x000005BD,
0x000005CE, 0x000005DF, 0x000005F0, 0x00000602, 0x00000614, 0x00000626, 0x00000638, 0x0000064A,
0x0000065D, 0x00000670, 0x00000683, 0x00000696, 0x000006AA, 0x000006BE, 0x000006D2, 0x000006E6,
0x000006FA, 0x0000070F, 0x00000724, 0x00000739, 0x0000074E, 0x00000764, 0x0000077A, 0x00000790,
0x000007A7, 0x000007BD, 0x000007D4, 0x000007EB, 0x00000803, 0x0000081B, 0x00000833, 0x0000084B,
0x00000863, 0x0000087C, 0x00000896, 0x000008AF, 0x000008C9, 0x000008E3, 0x000008FD, 0x00000918,
0x00000933, 0x0000094E, 0x0000096A, 0x00000985, 0x000009A2, 0x000009BE, 0x000009DB, 0x000009F8,
0x00000A16, 0x00000A34, 0x00000A52, 0x00000A71, 0x00000A90, 0x00000AAF, 0x00000ACE, 0x00000AEF,
0x00000B0F, 0x00000B30, 0x00000B51, 0x00000B72, 0x00000B94, 0x00000BB7, 0x00000BD9, 0x00000BFD,
0x00000C20, 0x00000C44, 0x00000C68, 0x00000C8D, 0x00000CB2, 0x00000CD8, 0x00000CFE, 0x00000D25,
0x00000D4C, 0x00000D73, 0x00000D9B, 0x00000DC3, 0x00000DEC, 0x00000E15, 0x00000E3F, 0x00000E69,
0x00000E94, 0x00000EBF, 0x00000EEB, 0x00000F17, 0x00000F44, 0x00000F71, 0x00000F9F, 0x00000FCD,
0x00000FFC, 0x0000102B, 0x0000105B, 0x0000108C, 0x000010BD, 0x000010EE, 0x00001121, 0x00001153,
0x00001187, 0x000011BB, 0x000011EF, 0x00001224, 0x0000125A, 0x00001291, 0x000012C8, 0x000012FF,
0x00001338, 0x00001371, 0x000013AA, 0x000013E4, 0x0000141F, 0x0000145B, 0x00001497, 0x000014D4,
0x00001512, 0x00001551, 0x00001590, 0x000015D0, 0x00001610, 0x00001652, 0x00001694, 0x000016D7,
0x0000171B, 0x0000175F, 0x000017A4, 0x000017EB, 0x00001831, 0x00001879, 0x000018C2, 0x0000190B,
0x00001955, 0x000019A0, 0x000019EC, 0x00001A39, 0x00001A87, 0x00001AD6, 0x00001B25, 0x00001B76,
0x00001BC7, 0x00001C19, 0x00001C6D, 0x00001CC1, 0x00001D16, 0x00001D6C, 0x00001DC4, 0x00001E1C,
0x00001E75, 0x00001ECF, 0x00001F2B, 0x00001F87, 0x00001FE5, 0x00002043, 0x000020A3, 0x00002103,
0x00002165, 0x000021C8, 0x0000222C, 0x00002292, 0x000022F8, 0x00002360, 0x000023C9, 0x00002433,
0x0000249E, 0x0000250B, 0x00002578, 0x000025E8, 0x00002658, 0x000026CA, 0x0000273D, 0x000027B1,
0x00002827, 0x0000289E, 0x00002916, 0x00002990, 0x00002A0B, 0x00002A88, 0x00002B06, 0x00002B85,
0x00002C06, 0x00002C89, 0x00002D0D, 0x00002D92, 0x00002E19, 0x00002EA2, 0x00002F2C, 0x00002FB8,
0x00003045, 0x000030D5, 0x00003165, 0x000031F8, 0x0000328C, 0x00003322, 0x000033B9, 0x00003453,
0x000034EE, 0x0000358B, 0x00003629, 0x000036CA, 0x0000376C, 0x00003811, 0x000038B7, 0x0000395F,
0x00003A09, 0x00003AB5, 0x00003B63, 0x00003C13, 0x00003CC5, 0x00003D79, 0x00003E30, 0x00003EE8,
0x00003FA2, 0x0000405F, 0x0000411E, 0x000041DF, 0x000042A2, 0x00004368, 0x0000442F, 0x000044FA,
0x000045C6, 0x00004695, 0x00004766, 0x0000483A, 0x00004910, 0x000049E8, 0x00004AC3, 0x00004BA1,
0x00004C81, 0x00004D64, 0x00004E49, 0x00004F32, 0x0000501C, 0x0000510A, 0x000051FA, 0x000052ED,
0x000053E3, 0x000054DC, 0x000055D7, 0x000056D6, 0x000057D7, 0x000058DB, 0x000059E3, 0x00005AED,
0x00005BFB, 0x00005D0B, 0x00005E1F, 0x00005F36, 0x00006050, 0x0000616E, 0x0000628F, 0x000063B3,
0x000064DA, 0x00006605, 0x00006734, 0x00006866, 0x0000699B, 0x00006AD4, 0x00006C11, 0x00006D51,
0x00006E95, 0x00006FDD, 0x00007129, 0x00007278, 0x000073CC, 0x00007523, 0x0000767E, 0x000077DD,
0x00007941, 0x00007AA8, 0x00007C14, 0x00007D83, 0x00007EF7, 0x00008070, 0x000081ED, 0x0000836E,
0x000084F3, 0x0000867D, 0x0000880C, 0x0000899F, 0x00008B37, 0x00008CD4, 0x00008E76, 0x0000901C,
0x000091C7, 0x00009377, 0x0000952C, 0x000096E6, 0x000098A6, 0x00009A6A, 0x00009C34, 0x00009E03,
0x00009FD7, 0x0000A1B1, 0x0000A391, 0x0000A575, 0x0000A760, 0x0000A950, 0x0000AB46, 0x0000AD42,
0x0000AF43, 0x0000B14B, 0x0000B358, 0x0000B56C, 0x0000B786, 0x0000B9A6, 0x0000BBCC, 0x0000BDF9,
0x0000C02C, 0x0000C266, 0x0000C4A6, 0x0000C6ED, 0x0000C93B, 0x0000CB8F, 0x0000CDEA, 0x0000D04D,
0x0000D2B6, 0x0000D527, 0x0000D79F, 0x0000DA1E, 0x0000DCA5, 0x0000DF33, 0x0000E1C8, 0x0000E466,
0x0000E70B, 0x0000E9B7, 0x0000EC6C, 0x0000EF29, 0x0000F1EE, 0x0000F4BB, 0x0000F791, 0x0000FA6F,
0x0000FD55, 0x00010044, 0x0001033C, 0x0001063C, 0x00010945, 0x00010C58, 0x00010F73, 0x00011298,
0x000115C6, 0x000118FD, 0x00011C3E, 0x00011F89, 0x000122DD, 0x0001263B, 0x000129A4, 0x00012D16,
0x00013092, 0x00013419, 0x000137AB, 0x00013B46, 0x00013EED, 0x0001429E, 0x0001465B, 0x00014A22,
0x00014DF5, 0x000151D3, 0x000155BC, 0x000159B1, 0x00015DB2, 0x000161BF, 0x000165D7, 0x000169FC,
0x00016E2D, 0x0001726B, 0x000176B5, 0x00017B0B, 0x00017F6F, 0x000183E0, 0x0001885D, 0x00018CE8,
0x00019181, 0x00019627, 0x00019ADB, 0x00019F9D, 0x0001A46D, 0x0001A94B, 0x0001AE38, 0x0001B333,
0x0001B83E, 0x0001BD57, 0x0001C27F, 0x0001C7B6, 0x0001CCFD, 0x0001D254, 0x0001D7BA, 0x0001DD30,
0x0001E2B7, 0x0001E84E, 0x0001EDF5, 0x0001F3AD, 0x0001F977, 0x0001FF51, 0x0002053D, 0x00020B3A,
0x00021149, 0x0002176A, 0x00021D9D, 0x000223E3, 0x00022A3B, 0x000230A6, 0x00023724, 0x00023DB5,
0x0002445A, 0x00024B12, 0x000251DE, 0x000258BF, 0x00025FB3, 0x000266BD, 0x00026DDB, 0x0002750F,
0x00027C57, 0x000283B6, 0x00028B2A, 0x000292B4, 0x00029A55, 0x0002A20C, 0x0002A9DA, 0x0002B1BF,
0x0002B9BC, 0x0002C1D0, 0x0002C9FD, 0x0002D241, 0x0002DA9E, 0x0002E314, 0x0002EBA3, 0x0002F44B,
0x0002FD0D, 0x000305E9, 0x00030EDF, 0x000317F0, 0x0003211B, 0x00032A62, 0x000333C4, 0x00033D42,
0x000346DC, 0x00035093, 0x00035A66, 0x00036457, 0x00036E65, 0x00037891, 0x000382DB, 0x00038D44,
0x000397CB, 0x0003A271, 0x0003AD38, 0x0003B81E, 0x0003C324, 0x0003CE4B, 0x0003D993, 0x0003E4FD,
0x0003F088, 0x0003FC36, 0x00040806, 0x000413F9, 0x00042010, 0x00042C4B, 0x000438A9, 0x0004452D,
0x000451D5, 0x00045EA4, 0x00046B98, 0x000478B2, 0x000485F3, 0x0004935C, 0x0004A0EC, 0x0004AEA5,
0x0004BC86, 0x0004CA90, 0x0004D8C4, 0x0004E722, 0x0004F5AB, 0x0005045F, 0x0005133E, 0x00052249,
0x00053181, 0x000540E6, 0x00055079, 0x0005603A, 0x0005702A, 0x00058048, 0x00059097, 0x0005A116,
0x0005B1C6, 0x0005C2A7, 0x0005D3BB, 0x0005E501, 0x0005F67A, 0x00060827, 0x00061A08, 0x00062C1F,
0x00063E6B, 0x000650ED, 0x000663A6, 0x00067697, 0x000689BF, 0x00069D21, 0x0006B0BC, 0x0006C491,
0x0006D8A1, 0x0006ECEC, 0x00070174, 0x00071638, 0x00072B3A, 0x0007407A, 0x000755FA, 0x00076BB9,
0x000781B8, 0x000797F9, 0x0007AE7B, 0x0007C541, 0x0007DC49, 0x0007F397, 0x00080B29, 0x00082301,
0x00083B20, 0x00085386, 0x00086C34, 0x0008852C, 0x00089E6E, 0x0008B7FA, 0x0008D1D3, 0x0008EBF8,
0x0009066A, 0x0009212B, 0x00093C3B, 0x0009579C, 0x0009734D, 0x00098F51, 0x0009ABA7, 0x0009C852,
0x0009E552, 0x000A02A7, 0x000A2054, 0x000A3E58, 0x000A5CB6, 0x000A7B6D, 0x000A9A80, 0x000AB9EF,
0x000AD9BB, 0x000AF9E5, 0x000B1A6E, 0x000B3B58, 0x000B5CA4, 0x000B7E52, 0x000BA064, 0x000BC2DB,
0x000BE5B8, 0x000C08FD, 0x000C2CAA, 0x000C50C1, 0x000C7543, 0x000C9A31, 0x000CBF8C, 0x000CE556,
0x000D0B91, 0x000D323C, 0x000D595A, 0x000D80ED, 0x000DA8F4, 0x000DD172, 0x000DFA69, 0x000E23D8,
0x000E4DC3, 0x000E7829, 0x000EA30E, 0x000ECE71, 0x000EFA55, 0x000F26BC, 0x000F53A6, 0x000F8115,
0x000FAF0A, 0x000FDD88, 0x00100C90, 0x00103C23, 0x00106C43, 0x00109CF2, 0x0010CE31, 0x00110003,
0x00113267, 0x00116562, 0x001198F3, 0x0011CD1D, 0x001201E2, 0x00123743, 0x00126D43, 0x0012A3E2,
0x0012DB24, 0x00131309, 0x00134B94, 0x001384C7, 0x0013BEA3, 0x0013F92B, 0x00143460, 0x00147044,
0x0014ACDB, 0x0014EA24, 0x00152824, 0x001566DB, 0x0015A64C, 0x0015E67A, 0x00162765, 0x00166911,
0x0016AB80, 0x0016EEB3, 0x001732AE, 0x00177772, 0x0017BD02, 0x00180361, 0x00184A90, 0x00189292,
0x0018DB69, 0x00192518, 0x00196FA2, 0x0019BB09, 0x001A074F, 0x001A5477, 0x001AA284, 0x001AF179,
0x001B4157, 0x001B9222, 0x001BE3DD, 0x001C368A, 0x001C8A2C, 0x001CDEC6, 0x001D345B, 0x001D8AED,
0x001DE280, 0x001E3B17, 0x001E94B4, 0x001EEF5B, 0x001F4B0F, 0x001FA7D2, 0x002005A9, 0x00206496,
0x0020C49C, 0x002125BE, 0x00218801, 0x0021EB67, 0x00224FF3, 0x0022B5AA, 0x00231C8E, 0x002384A3,
0x0023EDED, 0x0024586F, 0x0024C42C, 0x00253129, 0x00259F69, 0x00260EF0, 0x00267FC1, 0x0026F1E1,
0x00276553, 0x0027DA1C, 0x0028503E, 0x0028C7BF, 0x002940A2, 0x0029BAEB, 0x002A369F, 0x002AB3C1,
0x002B3257, 0x002BB263, 0x002C33EC, 0x002CB6F4, 0x002D3B81, 0x002DC196, 0x002E4939, 0x002ED26E,
0x002F5D3A, 0x002FE9A2, 0x003077A9, 0x00310756, 0x003198AC, 0x00322BB1, 0x0032C06A, 0x003356DC,
0x0033EF0C, 0x003488FF, 0x003524BB, 0x0035C244, 0x003661A0, 0x003702D4, 0x0037A5E6, 0x00384ADC,
0x0038F1BB, 0x00399A88, 0x003A454A, 0x003AF206, 0x003BA0C2, 0x003C5184, 0x003D0452, 0x003DB932,
0x003E702A, 0x003F2940, 0x003FE47B, 0x0040A1E2, 0x00416179, 0x00422349, 0x0042E757, 0x0043ADAA,
0x00447649, 0x0045413B, 0x00460E87, 0x0046DE33, 0x0047B046, 0x004884C9, 0x00495BC1, 0x004A3537,
0x004B1131, 0x004BEFB7, 0x004CD0D1, 0x004DB486, 0x004E9ADE, 0x004F83E1, 0x00506F97, 0x00515E08,
0x00524F3B, 0x00534339, 0x00543A0B, 0x005533B8, 0x00563049, 0x00572FC8, 0x0058323B, 0x005937AD,
0x005A4025, 0x005B4BAE, 0x005C5A4F, 0x005D6C13, 0x005E8102, 0x005F9927, 0x0060B48A, 0x0061D334,
0x0062F531, 0x00641A89, 0x00654347, 0x00666F74, 0x00679F1C, 0x0068D247, 0x006A0901, 0x006B4354,
0x006C814B, 0x006DC2F0, 0x006F084F, 0x00705172, 0x00719E65, 0x0072EF33, 0x007443E8, 0x00759C8E,
0x0076F932, 0x007859DF, 0x0079BEA2, 0x007B2787, 0x007C9499, 0x007E05E6, 0x007F7B79, 0x0080F560,
0x008273A6, 0x0083F65A, 0x00857D89, 0x0087093F, 0x0088998A, 0x008A2E77, 0x008BC815, 0x008D6672,
0x008F099A, 0x0090B19D, 0x00925E89, 0x0094106D, 0x0095C756, 0x00978355, 0x00994478, 0x009B0ACE,
0x009CD667, 0x009EA752, 0x00A07DA0, 0x00A25960, 0x00A43AA2, 0x00A62177, 0x00A80DEE, 0x00AA001A,
0x00ABF80A, 0x00ADF5D1, 0x00AFF97E, 0x00B20324, 0x00B412D4, 0x00B628A1, 0x00B8449C, 0x00BA66D8,
0x00BC8F67, 0x00BEBE5B, 0x00C0F3C9, 0x00C32FC3, 0x00C5725D, 0x00C7BBA9, 0x00CA0BBD, 0x00CC62AC,
0x00CEC08A, 0x00D1256C, 0x00D39167, 0x00D60490, 0x00D87EFC, 0x00DB00C0, 0x00DD89F3, 0x00E01AAB,
0x00E2B2FD, 0x00E55300, 0x00E7FACC, 0x00EAAA77, 0x00ED6218, 0x00F021C7, 0x00F2E99C, 0x00F5B9B0,
0x00F89219, 0x00FB72F2, 0x00FE5C54, 0x01014E57, 0x01044915, 0x01074CA8, 0x010A592A, 0x010D6EB6,
0x01108D67, 0x0113B557, 0x0116E6A2, 0x011A2164, 0x011D65B9, 0x0120B3BC, 0x01240B8C, 0x01276D45,
0x012AD904, 0x012E4EE7, 0x0131CF0B, 0x01355991, 0x0138EE96, 0x013C8E39, 0x0140389A, 0x0143EDD8,
0x0147AE14, 0x014B796F, 0x014F500A, 0x01533205, 0x01571F82, 0x015B18A5, 0x015F1D8E, 0x01632E61,
0x01674B42, 0x016B7454, 0x016FA9BB, 0x0173EB9C, 0x01783A1B, 0x017C955F, 0x0180FD8D, 0x018572CB,
0x0189F540, 0x018E8513, 0x0193226D, 0x0197CD74, 0x019C8651, 0x01A14D2E, 0x01A62234, 0x01AB058D,
0x01AFF764, 0x01B4F7E3, 0x01BA0735, 0x01BF2588, 0x01C45306, 0x01C98FDE, 0x01CEDC3D, 0x01D43850,
0x01D9A447, 0x01DF2050, 0x01E4AC9B, 0x01EA4958, 0x01EFF6B8, 0x01F5B4ED, 0x01FB8428, 0x0201649B,
0x0207567A, 0x020D59F9, 0x02136F4B, 0x021996A5, 0x021FD03D, 0x02261C4A, 0x022C7B01, 0x0232EC9A,
0x0239714D, 0x02400952, 0x0246B4E4, 0x024D743B, 0x02544792, 0x025B2F26, 0x02622B31, 0x02693BF0,
0x027061A1, 0x02779C82, 0x027EECD2, 0x028652D0, 0x028DCEBC, 0x029560D8, 0x029D0964, 0x02A4C8A5,
0x02AC9EDD, 0x02B48C50, 0x02BC9142, 0x02C4ADFB, 0x02CCE2BF, 0x02D52FD7, 0x02DD958A, 0x02E61422,
0x02EEABE8, 0x02F75D27, 0x0300282A, 0x03090D3F, 0x03120CB1, 0x031B26CF, 0x03245BE9, 0x032DAC4D,
0x0337184E, 0x0340A03D, 0x034A446D, 0x03540531, 0x035DE2DF, 0x0367DDCC, 0x0371F64E, 0x037C2CBD,
0x03868173, 0x0390F4C8, 0x039B8719, 0x03A638BF, 0x03B10A19, 0x03BBFB84, 0x03C70D60, 0x03D2400C,
0x03DD93E9, 0x03E9095B, 0x03F4A0C5, 0x04005A8B, 0x040C3714, 0x041836C5, 0x04245A09, 0x0430A147,
0x043D0CEB, 0x04499D60, 0x04565314, 0x04632E76, 0x04702FF4, 0x047D57FF, 0x048AA70B, 0x04981D8B,
0x04A5BBF3, 0x04B382B9, 0x04C17257, 0x04CF8B44, 0x04DDCDFB, 0x04EC3AF8, 0x04FAD2B9, 0x050995BB,
0x05188480, 0x05279F89, 0x0536E758, 0x05465C74, 0x0555FF62, 0x0565D0AB, 0x0575D0D6, 0x05860070,
0x05966005, 0x05A6F023, 0x05B7B15B, 0x05C8A43D, 0x05D9C95D, 0x05EB2150, 0x05FCACAD, 0x060E6C0B,
0x06206006, 0x06328938, 0x0644E841, 0x06577DBE, 0x066A4A53, 0x067D4EA2, 0x06908B50, 0x06A40104,
0x06B7B068, 0x06CB9A26, 0x06DFBEEC, 0x06F41F68, 0x0708BC4C, 0x071D964A, 0x0732AE18, 0x0748046D,
0x075D9A02, 0x07736F92, 0x078985DC, 0x079FDD9F, 0x07B6779E, 0x07CD549C, 0x07E47560, 0x07FBDAB4,
0x08138562, 0x082B7638, 0x0843AE06, 0x085C2D9E, 0x0874F5D6, 0x088E0783, 0x08A76381, 0x08C10AAC,
0x08DAFDE2, 0x08F53E04, 0x090FCBF7, 0x092AA8A2, 0x0945D4EE, 0x096151C6, 0x097D201A, 0x099940DB,
0x09B5B4FE, 0x09D27D79, 0x09EF9B47, 0x0A0D0F64, 0x0A2ADAD1, 0x0A48FE91, 0x0A677BA8, 0x0A865320,
0x0AA58606, 0x0AC51567, 0x0AE50256, 0x0B054DE8, 0x0B25F937, 0x0B47055D, 0x0B68737A, 0x0B8A44AF,
0x0BAC7A24, 0x0BCF1501, 0x0BF21673, 0x0C157FA9, 0x0C3951D8, 0x0C5D8E36, 0x0C8235FF, 0x0CA74A70,
0x0CCCCCCD, 0x0CF2BE5A, 0x0D192061, 0x0D3FF430, 0x0D673B17, 0x0D8EF66D, 0x0DB7278B, 0x0DDFCFCC,
0x0E08F094, 0x0E328B46, 0x0E5CA14C, 0x0E873415, 0x0EB24511, 0x0EDDD5B7, 0x0F09E781, 0x0F367BEE,
0x0F639481, 0x0F9132C3, 0x0FBF583F, 0x0FEE0686, 0x101D3F2D, 0x104D03D0, 0x107D560D, 0x10AE3787,
0x10DFA9E7, 0x1111AEDB, 0x11444815, 0x1177774D, 0x11AB3E3F, 0x11DF9EAE, 0x12149A60, 0x124A3321,
0x12806AC3, 0x12B7431D, 0x12EEBE0C, 0x1326DD70, 0x135FA333, 0x13991141, 0x13D3298C, 0x140DEE0E,
0x144960C5, 0x148583B6, 0x14C258EA, 0x14FFE273, 0x153E2266, 0x157D1AE2, 0x15BCCE07, 0x15FD3E01,
0x163E6CFE, 0x16805D35, 0x16C310E3, 0x17068A4B, 0x174ACBB8, 0x178FD779, 0x17D5AFE8, 0x181C5762,
0x1863D04D, 0x18AC1D17, 0x18F54033, 0x193F3C1D, 0x198A1357, 0x19D5C86C, 0x1A225DED, 0x1A6FD673,
0x1ABE349F, 0x1B0D7B1B, 0x1B5DAC97, 0x1BAECBCA, 0x1C00DB77, 0x1C53DE66, 0x1CA7D768, 0x1CFCC956,
0x1D52B712, 0x1DA9A387, 0x1E0191A9, 0x1E5A8471, 0x1EB47EE7, 0x1F0F8416, 0x1F6B9715, 0x1FC8BB06,
0x2026F30F, 0x20864265, 0x20E6AC43, 0x214833EE, 0x21AADCB6, 0x220EA9F4, 0x22739F0A, 0x22D9BF65,
0x23410E7E, 0x23A98FD5, 0x241346F6, 0x247E3777, 0x24EA64F9, 0x2557D328, 0x25C685BB, 0x26368073,
0x26A7C71D, 0x271A5D91, 0x278E47B3, 0x28038970, 0x287A26C4, 0x28F223B6, 0x296B8457, 0x29E64CC5,
0x2A62812C, 0x2AE025C3, 0x2B5F3ECC, 0x2BDFD098, 0x2C61DF84, 0x2CE56FF9, 0x2D6A866F, 0x2DF12769,
0x2E795779, 0x2F031B3E, 0x2F8E7765, 0x301B70A8, 0x30AA0BCF, 0x313A4DB3, 0x31CC3B37, 0x325FD94F,
0x32F52CFF, 0x338C3B56, 0x34250975, 0x34BF9C8B, 0x355BF9D8, 0x35FA26A9, 0x369A285D, 0x373C0461,
0x37DFC033, 0x38856163, 0x392CED8E, 0x39D66A63, 0x3A81DDA4, 0x3B2F4D22, 0x3BDEBEBF, 0x3C90386F,
0x3D43C038, 0x3DF95C32, 0x3EB11285, 0x3F6AE96F, 0x4026E73C, 0x40E5124F, 0x41A5711B, 0x42680A28,
0x432CE40F, 0x43F4057E, 0x44BD7539, 0x45893A13, 0x46575AF8, 0x4727DEE6, 0x47FACCF0, 0x48D02C3F,
0x49A8040F, 0x4A825BB5, 0x4B5F3A99, 0x4C3EA838, 0x4D20AC29, 0x4E054E17, 0x4EEC95C3, 0x4FD68B07,
0x50C335D3, 0x51B29E2F, 0x52A4CC3A, 0x5399C82D, 0x54919A57, 0x558C4B22, 0x5689E30E, 0x578A6AB7,
0x588DEAD1, 0x59946C2A, 0x5A9DF7AB, 0x5BAA9656, 0x5CBA514A, 0x5DCD31BD, 0x5EE34105, 0x5FFC8890,
0x611911E9, 0x6238E6BA, 0x635C10C5, 0x648299EC, 0x65AC8C2E, 0x66D9F1A7, 0x680AD491, 0x693F3F45,
0x6A773C39, 0x6BB2D603, 0x6CF2175A, 0x6E350B13, 0x6F7BBC23, 0x70C6359F, 0x721482BF, 0x7366AEDB,
0x74BCC56B, 0x7616D20D, 0x7774E07D, 0x78D6FC9E, 0x7A3D3271, 0x7BA78E21, 0x7D161BF7, 0x7E88E865,
0x7FFFFFFF};

/* Update the count of streams for which D0i3 can be attempted and that of
 * streams for which D0i3 can not be attempted. In the streaming case
 * D0i3 can be attempted for streams that can afford large latency/large
 * DMA FIFO */
void skl_tplg_update_d0i3_stream_count(struct snd_soc_dai *dai, bool open)
{

	struct skl *skl = get_skl_ctx(dai->dev);
	struct skl_d0i3_data *d0i3_data =  &skl->skl_sst->d0i3_data;

	if (open) {
		if (!strncmp(dai->name, "Deepbuffer Pin", 14))
			d0i3_data->d0i3_stream_count++;
		else
			d0i3_data->non_d0i3_stream_count++;
	} else {
		if (!strncmp(dai->name, "Deepbuffer Pin", 14))
			d0i3_data->d0i3_stream_count--;
		else
			d0i3_data->non_d0i3_stream_count--;
	}
	dev_dbg(dai->dev, "%s:d0i3_count= %d ; non_d0i3_count= %d\n", __func__,
			d0i3_data->d0i3_stream_count, d0i3_data->non_d0i3_stream_count);
}

/*
 * SKL DSP driver modelling uses only few DAPM widgets so for rest we will
 * ignore. This helpers checks if the SKL driver handles this widget type
 */
int is_skl_dsp_widget_type(struct snd_soc_dapm_widget *w)
{
	switch (w->id) {
	case snd_soc_dapm_dai_link:
	case snd_soc_dapm_dai_in:
	case snd_soc_dapm_aif_in:
	case snd_soc_dapm_aif_out:
	case snd_soc_dapm_dai_out:
	case snd_soc_dapm_switch:
		return false;
	default:
		return true;
	}
}

/*
 * Each pipelines needs memory to be allocated. Check if we have free memory
 * from available pool.
 */
static bool skl_is_pipe_mem_avail(struct skl *skl,
				struct skl_module_cfg *mconfig)
{
	struct skl_sst *ctx = skl->skl_sst;

	if (skl->resource.mem + mconfig->pipe->memory_pages >
				skl->resource.max_mem) {
		dev_err(ctx->dev,
				"%s: module_id %d instance %d\n", __func__,
				mconfig->id.module_id,
				mconfig->id.instance_id);
		dev_err(ctx->dev,
				"exceeds ppl memory available %d mem %d\n",
				skl->resource.max_mem, skl->resource.mem);
		return false;
	} else {
		return true;
	}
}

/*
 * Add the mem to the mem pool. This is freed when pipe is deleted.
 * Note: DSP does actual memory management we only keep track for complete
 * pool
 */
static void skl_tplg_alloc_pipe_mem(struct skl *skl,
				struct skl_module_cfg *mconfig)
{
	skl->resource.mem += mconfig->pipe->memory_pages;
}

/*
 * Pipeline needs needs DSP CPU resources for computation, this is
 * quantified in MCPS (Million Clocks Per Second) required for module/pipe
 *
 * Each pipelines needs mcps to be allocated. Check if we have mcps for this
 * pipe.
 */

static bool skl_is_pipe_mcps_avail(struct skl *skl,
				struct skl_module_cfg *mconfig)
{
	struct skl_sst *ctx = skl->skl_sst;

	if (skl->resource.mcps + mconfig->mcps > skl->resource.max_mcps) {
		dev_err(ctx->dev,
			"%s: module_id %d instance %d\n", __func__,
			mconfig->id.module_id, mconfig->id.instance_id);
		dev_err(ctx->dev,
			"exceeds ppl mcps available %d > mem %d\n",
			skl->resource.max_mcps, skl->resource.mcps);
		return false;
	} else {
		return true;
	}
}

static void skl_tplg_alloc_pipe_mcps(struct skl *skl,
				struct skl_module_cfg *mconfig)
{
	skl->resource.mcps += mconfig->mcps;
}

/*
 * Free the mcps when tearing down
 */
static void
skl_tplg_free_pipe_mcps(struct skl *skl, struct skl_module_cfg *mconfig)
{
	skl->resource.mcps -= mconfig->mcps;
}

/*
 * Free the memory when tearing down
 */
static void
skl_tplg_free_pipe_mem(struct skl *skl, struct skl_module_cfg *mconfig)
{
	skl->resource.mem -= mconfig->pipe->memory_pages;
}


static void skl_dump_mconfig(struct skl_sst *ctx,
					struct skl_module_cfg *mcfg)
{
	dev_dbg(ctx->dev, "Dumping config\n");
	dev_dbg(ctx->dev, "Input Format:\n");
	dev_dbg(ctx->dev, "channels = %d\n", mcfg->in_fmt[0].channels);
	dev_dbg(ctx->dev, "s_freq = %d\n", mcfg->in_fmt[0].s_freq);
	dev_dbg(ctx->dev, "ch_cfg = %d\n", mcfg->in_fmt[0].ch_cfg);
	dev_dbg(ctx->dev, "valid bit depth = %d\n", mcfg->in_fmt[0].valid_bit_depth);
	dev_dbg(ctx->dev, "Output Format:\n");
	dev_dbg(ctx->dev, "channels = %d\n", mcfg->out_fmt[0].channels);
	dev_dbg(ctx->dev, "s_freq = %d\n", mcfg->out_fmt[0].s_freq);
	dev_dbg(ctx->dev, "valid bit depth = %d\n", mcfg->out_fmt[0].valid_bit_depth);
	dev_dbg(ctx->dev, "ch_cfg = %d\n", mcfg->out_fmt[0].ch_cfg);
}

static void skl_tplg_update_params(struct skl_module_fmt *fmt,
			struct skl_pipe_params *params, int fixup)
{
	if (fixup & SKL_RATE_FIXUP_MASK)
		fmt->s_freq = params->s_freq;
	if (fixup & SKL_CH_FIXUP_MASK) {
		fmt->channels = params->ch;
		if (fmt->channels == 1) {
			fmt->ch_cfg = SKL_CH_CFG_MONO;
			fmt->ch_map = 0xfffffff0;
		} else if (fmt->channels == 2) {
			fmt->ch_cfg = SKL_CH_CFG_STEREO;
			fmt->ch_map = 0xffffff10;
		}
	}
	if (fixup & SKL_FMT_FIXUP_MASK)
		fmt->valid_bit_depth = skl_get_bit_depth(params->s_fmt);

	/*
	 * 16 bit is 16 bit container whereas 24 bit is in 32 bit container so
	 * update bit depth accordingly
	 */
	if (fixup & SKL_FMT_FIXUP_MASK) {
		switch (fmt->valid_bit_depth) {
		case SKL_DEPTH_16BIT:
			fmt->bit_depth = fmt->valid_bit_depth;
			break;

		case SKL_DEPTH_24BIT:
		case SKL_DEPTH_32BIT:
			fmt->bit_depth = SKL_DEPTH_32BIT;
			break;
		}
	}

}

/*
 * A pipeline may have modules which impact the pcm parameters, like SRC,
 * channel converter, format converter.
 * We need to calculate the output params by applying the 'fixup'
 * Topology will tell driver which type of fixup is to be applied by
 * supplying the fixup mask, so based on that we calculate the output
 *
 * Now In FE the pcm hw_params is source/target format. Same is applicable
 * for BE with its hw_params invoked.
 * here based on FE, BE pipeline and direction we calculate the input and
 * outfix and then apply that for a module
 */
static void skl_tplg_update_params_fixup(struct skl_module_cfg *m_cfg,
		struct skl_pipe_params *params, bool is_fe)
{
	int in_fixup, out_fixup;
	struct skl_module_fmt *in_fmt, *out_fmt;

	/* Fixups will be applied to pin 0 only */
	in_fmt = &m_cfg->in_fmt[0];
	out_fmt = &m_cfg->out_fmt[0];

	if (params->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (is_fe) {
			in_fixup = m_cfg->params_fixup;
			out_fixup = (~m_cfg->converter) &
					m_cfg->params_fixup;
		} else {
			out_fixup = m_cfg->params_fixup;
			in_fixup = (~m_cfg->converter) &
					m_cfg->params_fixup;
		}
	} else {
		if (is_fe) {
			out_fixup = m_cfg->params_fixup;
			in_fixup = (~m_cfg->converter) &
					m_cfg->params_fixup;
		} else {
			in_fixup = m_cfg->params_fixup;
			out_fixup = (~m_cfg->converter) &
					m_cfg->params_fixup;
		}
	}

	skl_tplg_update_params(in_fmt, params, in_fixup);
	skl_tplg_update_params(out_fmt, params, out_fixup);
}

/*
 * A module needs input and output buffers, which are dependent upon pcm
 * params, so once we have calculate params, we need buffer calculation as
 * well.
 */
static void skl_tplg_update_buffer_size(struct skl_sst *ctx,
				struct skl_module_cfg *mcfg)
{
	struct skl_module_fmt *in_fmt, *out_fmt;

	/* Since fixups is applied to pin 0 only, ibs, obs needs
	 * change for pin 0 only
	 */
	in_fmt = &mcfg->in_fmt[0];
	out_fmt = &mcfg->out_fmt[0];
	mcfg->ibs = ((in_fmt->s_freq + 999) / 1000) *
				(mcfg->in_fmt->channels) *
				(mcfg->in_fmt->bit_depth >> 3) *
				mcfg->in_frame_size;

	mcfg->obs = ((mcfg->out_fmt->s_freq + 999) / 1000) *
				(mcfg->out_fmt->channels) *
				(mcfg->out_fmt->bit_depth >> 3) *
				mcfg->out_frame_size;
}

static int skl_tplg_update_be_blob(struct snd_soc_dapm_widget *w,
						struct skl_sst *ctx)
{
	struct skl_module_cfg *m_cfg = w->priv;
	int link_type, dir;
	u32 ch, s_freq, s_fmt;
	struct nhlt_specific_cfg *cfg;
	struct skl *skl = get_skl_ctx(ctx->dev);

	/* check if we already have blob */
	if (m_cfg->formats_config.caps_size > 0)
		return 0;

	switch (m_cfg->dev_type) {
	case SKL_DEVICE_DMIC:
		link_type = NHLT_LINK_DMIC;
		dir = 1;
		s_freq = m_cfg->in_fmt[0].s_freq;
		s_fmt = m_cfg->in_fmt[0].bit_depth;
		ch = m_cfg->in_fmt[0].channels;
		break;

	case SKL_DEVICE_I2S:
		link_type = NHLT_LINK_SSP;
		if (m_cfg->hw_conn_type == SKL_CONN_SOURCE) {
			dir = 1;
			s_freq = m_cfg->in_fmt[0].s_freq;
			s_fmt = m_cfg->in_fmt[0].bit_depth;
			ch = m_cfg->in_fmt[0].channels;
		} else {
			dir = 0;
			s_freq = m_cfg->out_fmt[0].s_freq;
			s_fmt = m_cfg->out_fmt[0].bit_depth;
			ch = m_cfg->out_fmt[0].channels;
		}
		break;

	default:
		return -EINVAL;
	}

	/* update the blob based on virtual bus_id and default params */
	cfg = skl_get_ep_blob(skl, m_cfg->vbus_id, link_type,
					s_fmt, ch, s_freq, dir);
	if (cfg) {
		m_cfg->formats_config.caps_size = cfg->size;
		m_cfg->formats_config.caps = (u32 *) &cfg->caps;
	} else {
		dev_err(ctx->dev, "Blob NULL for id %x type %d dirn %d\n",
					m_cfg->vbus_id, link_type, dir);
		dev_err(ctx->dev, "PCM: ch %d, freq %d, fmt %d\n",
					ch, s_freq, s_fmt);
		return -EIO;
	}

	return 0;
}

static void skl_tplg_update_module_params(struct snd_soc_dapm_widget *w,
							struct skl_sst *ctx)
{
	struct skl_module_cfg *m_cfg = w->priv;
	struct skl_pipe_params *params = m_cfg->pipe->p_params;
	int p_conn_type = m_cfg->pipe->conn_type;
	bool is_fe;

	if (!m_cfg->params_fixup)
		return;

	dev_dbg(ctx->dev, "Mconfig for widget=%s BEFORE updation\n",
				w->name);

	skl_dump_mconfig(ctx, m_cfg);

	if (p_conn_type == SKL_PIPE_CONN_TYPE_FE)
		is_fe = true;
	else
		is_fe = false;

	skl_tplg_update_params_fixup(m_cfg, params, is_fe);
	skl_tplg_update_buffer_size(ctx, m_cfg);

	dev_dbg(ctx->dev, "Mconfig for widget=%s AFTER updation\n",
				w->name);

	skl_dump_mconfig(ctx, m_cfg);
}

/*
 * A pipe can have multiple modules, each of them will be a DAPM widget as
 * well. While managing a pipeline we need to get the list of all the
 * widgets in a pipelines, so this helper - skl_tplg_get_pipe_widget() helps
 * to get the SKL type widgets in that pipeline
 */
static int skl_tplg_alloc_pipe_widget(struct device *dev,
	struct snd_soc_dapm_widget *w, struct skl_pipe *pipe)
{
	struct skl_module_cfg *src_module = NULL;
	struct snd_soc_dapm_path *p = NULL;
	struct skl_pipe_module *p_module = NULL;

	p_module = devm_kzalloc(dev, sizeof(*p_module), GFP_KERNEL);
	if (!p_module)
		return -ENOMEM;

	p_module->w = w;
	list_add_tail(&p_module->node, &pipe->w_list);

	snd_soc_dapm_widget_for_each_sink_path(w, p) {
		if ((p->sink->priv == NULL)
				&& (!is_skl_dsp_widget_type(w)))
			continue;

		if ((p->sink->priv != NULL) && p->connect
				&& is_skl_dsp_widget_type(p->sink)) {

			src_module = p->sink->priv;
			if (pipe->ppl_id == src_module->pipe->ppl_id)
				skl_tplg_alloc_pipe_widget(dev,
							p->sink, pipe);
		}
	}
	return 0;
}

int skl_get_probe_index(struct snd_soc_dai *dai,
				struct skl_probe_config *pconfig)
{
	int i, ret = -1;
	char pos[4];

	for (i = 0; i < pconfig->no_injector; i++) {
		snprintf(pos, 4, "%d", i);
		if (strstr(dai->name, pos))
			return i;
	}
	return ret;
}

int skl_tplg_attach_probe_dma(struct snd_soc_dapm_widget *w,
					struct skl_sst *ctx, struct snd_soc_dai *dai)
{
	int i, ret;
	struct skl_module_cfg *mconfig = w->priv;
	struct skl_attach_probe_dma ad;
	struct skl_probe_config *pconfig = &ctx->probe_config;

	if ((i = skl_get_probe_index(dai, pconfig)) != -1) {
		ad.node_id.node.vindex = pconfig->iprobe[i].dma_id;
		ad.node_id.node.dma_type = SKL_DMA_HDA_HOST_OUTPUT_CLASS;
		ad.node_id.node.rsvd = 0;
		ad.dma_buff_size = 1536;/* TODO:Configure based on calculation*/
	}

	ret = skl_set_module_params(ctx, &ad, sizeof(struct skl_attach_probe_dma),
						1, mconfig);
	return ret;

}

int skl_tplg_set_probe_params(struct snd_soc_dapm_widget *w,
					struct skl_sst *ctx, int direction,
					struct snd_soc_dai *dai)
{
	int i, ret = 0, n = 0;
	struct skl_module_cfg *mconfig = w->priv;
	const struct snd_kcontrol_new *k;
	struct soc_bytes_ext *sb;
	struct skl_probe_data *bc;
	struct skl_probe_config *pconfig = &ctx->probe_config;
	struct probe_pt_param prb_pt_param[8] = {0};

	if (direction == SND_COMPRESS_PLAYBACK) {

		/* only one injector point can be set at a time*/
		n = skl_get_probe_index(dai, pconfig);
		if (n < 0)
			return -EINVAL;

		k = &w->kcontrol_news[pconfig->no_extractor + n];

		if (k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
			sb = (void *) k->private_value;
			bc = (struct skl_probe_data *)sb->dobj.private;
			pr_debug("bc->is_ext_inj = %d, bc->params = %d, bc->is_connect = %d \n",
						bc->is_ext_inj, bc->params, bc->is_connect);
			if (!(bc->is_ext_inj == SKL_PROBE_INJECT ||
					bc->is_ext_inj == SKL_PROBE_INJECT_REEXTRACT))
				return -EINVAL;

			prb_pt_param[0].params = (int)bc->params;
			prb_pt_param[0].connection = bc->is_ext_inj;
			prb_pt_param[0].node_id =  pconfig->iprobe[n].dma_id;
			ret = skl_set_module_params(ctx, (void *)prb_pt_param, sizeof(struct probe_pt_param),
							bc->is_connect, mconfig);
		}

	} else if (direction == SND_COMPRESS_CAPTURE) {

		/*multiple extractor points can be set simultaneously*/
		for (i = 0; i < pconfig->no_extractor; i++) {
			k = &w->kcontrol_news[i];
			if (k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
				sb = (void *) k->private_value;
				bc = (struct skl_probe_data *)sb->dobj.private;

				pr_debug("bc->is_ext_inj = %d, bc->params = %d, bc->is_connect = %d \n",
							bc->is_ext_inj, bc->params, bc->is_connect);
				if (bc->is_ext_inj == SKL_PROBE_EXTRACT &&
						pconfig->eprobe[i].set == 1) {
					pr_debug("Retrieving the exractor params \n");
					prb_pt_param[n].params = (int)bc->params;
					prb_pt_param[n].connection = bc->is_ext_inj;
					prb_pt_param[n].node_id = -1;
					n++;
				}
			}
		}
		ret = skl_set_module_params(ctx, (void *)prb_pt_param, n * sizeof(struct probe_pt_param),
					SKL_PROBE_CONNECT, mconfig);

	}
	return ret;
}

/*
 * some modules can have mutiple params set from user control and
 * need to be set after module is initalized. if set_param flag is
 * set module params will be done after module is initalised.
 */
int skl_tplg_set_module_params(struct snd_soc_dapm_widget *w,
						struct skl_sst *ctx)
{
	int i, ret;
	struct skl_module_cfg *mconfig = w->priv;
	const struct snd_kcontrol_new *k;
	struct soc_bytes_ext *sb;
	struct skl_algo_data *bc;
	struct skl_specific_cfg *sp_cfg;

	if (mconfig->formats_config.caps_size > 0 &&
		mconfig->formats_config.set_params == SKL_PARAM_SET) {
		sp_cfg = &mconfig->formats_config;
		ret = skl_set_module_params(ctx, (void *)sp_cfg->caps,
							sp_cfg->caps_size,
							sp_cfg->param_id,
							mconfig);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < w->num_kcontrols; i++) {
		k = &w->kcontrol_news[i];
		if (k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
			sb = (void *) k->private_value;
			bc = (struct skl_algo_data *)sb->dobj.private;

			if (bc->set_params == SKL_PARAM_SET) {
				ret = skl_set_module_params(ctx,
							(void *)bc->params,
							bc->max,
							bc->param_id,
							mconfig);
				if (ret < 0)
					return ret;
			}
		}
	}

	return 0;
}

/*
 * some module param can set from user control and this is required as
 * when module is initailzed. if module param is required in init it is
 * identifed by set_param flag. if set_param flag is not set, then this
 * parameter needs to set as part of module init.
 */
static int skl_tplg_set_module_init_data(struct snd_soc_dapm_widget *w)
{
	const struct snd_kcontrol_new *k;
	struct soc_bytes_ext *sb;
	struct skl_algo_data *bc;
	struct skl_module_cfg *mconfig = w->priv;
	int i;

	for (i = 0; i < w->num_kcontrols; i++) {
		k = &w->kcontrol_news[i];
		if (k->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
			sb = (void *) k->private_value;
			bc = (struct skl_algo_data *)sb->dobj.private;
			if (bc->set_params != SKL_PARAM_INIT)
				continue;
			mconfig->formats_config.caps = (u32 *)bc->params;
			mconfig->formats_config.caps_size = bc->max;
			break;
		}
	}

	return 0;
}

/*
 * Inside a pipe instance, we can have various modules. These modules need
 * to instantiated in DSP by invoking INIT_MODULE IPC, which is achieved by
 * skl_init_module() routine, so invoke that for all modules in a pipeline
 */
static int
skl_tplg_init_pipe_modules(struct skl *skl, struct skl_pipe *pipe)
{
	struct skl_pipe_module *w_module;
	struct snd_soc_dapm_widget *w;
	struct skl_module_cfg *mconfig;
	struct skl_sst *ctx = skl->skl_sst;
	int ret = 0;

	list_for_each_entry(w_module, &pipe->w_list, node) {
		w = w_module->w;
		mconfig = w->priv;

		/* check resource available */
		if (!skl_is_pipe_mcps_avail(skl, mconfig))
			return -ENOMEM;

		if (mconfig->is_loadable) {
			ret = skl_load_modules(ctx, mconfig);
			if (ret < 0)
				return ret;
		}

		/* update blob if blob is null for be with default value */
		skl_tplg_update_be_blob(w, ctx);

		/*
		 * apply fix/conversion to module params based on
		 * FE/BE params
		 */
		skl_tplg_update_module_params(w, ctx);

		skl_tplg_set_module_init_data(w);

		ret = skl_dsp_get_core(ctx->dsp, mconfig->core_id);

		if (ret < 0) {
			dev_err(ctx->dev, "Failed to wake up core %d ret=%d\n",
						mconfig->core_id, ret);
			return ret;
		}

		ret = skl_init_module(ctx, mconfig);
		if (ret < 0)
			return ret;

		ret = skl_tplg_set_module_params(w, ctx);
		if (ret < 0)
			return ret;
		skl_tplg_alloc_pipe_mcps(skl, mconfig);
	}

	return 0;
}

static int skl_tplg_unload_pipe_modules(struct skl_sst *ctx,
	 struct skl_pipe *pipe)
{
	struct skl_pipe_module *w_module = NULL;
	struct skl_module_cfg *mconfig = NULL;
	int ret = 0;

	dev_dbg(ctx->dev, "%s: pipe=%d\n", __func__, pipe->ppl_id);
	list_for_each_entry(w_module, &pipe->w_list, node) {
		mconfig  = w_module->w->priv;
		dev_dbg(ctx->dev, "unload module_id=%d\n", mconfig->id.module_id);

		if (mconfig->is_loadable)
			ret = skl_unload_modules(ctx, mconfig);

		ret = skl_dsp_put_core(ctx->dsp, mconfig->core_id);
		if (ret < 0) {
			/* notify error, continue with other modules */
			dev_err(ctx->dev, "Failed to sleep core %d ret=%d\n",
					mconfig->core_id, ret);

		}
	}

	return ret;
}

/*
 * Mixer module represents a pipeline. So in the Pre-PMU event of mixer we
 * need create the pipeline. So we do following:
 *   - check the resources
 *   - Create the pipeline
 *   - Initialize the modules in pipeline
 *   - finally bind all modules together
 */
static int skl_tplg_mixer_dapm_pre_pmu_event(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	int ret;
	struct skl_module_cfg *mconfig = w->priv;
	struct skl_pipe_module *w_module;
	struct skl_pipe *s_pipe = mconfig->pipe;
	struct skl_module_cfg *src_module = NULL, *dst_module;
	struct skl_sst *ctx = skl->skl_sst;

	/* check resource available */
	if (!skl_is_pipe_mcps_avail(skl, mconfig))
		return -EBUSY;

	if (!skl_is_pipe_mem_avail(skl, mconfig))
		return -ENOMEM;

	/*
	 * Create a list of modules for pipe.
	 * This list contains modules from source to sink
	 */
	ret = skl_create_pipeline(ctx, mconfig->pipe);
	if (ret < 0)
		return ret;

	/*
	 * we create a w_list of all widgets in that pipe. This list is not
	 * freed on PMD event as widgets within a pipe are static. This
	 * saves us cycles to get widgets in pipe every time.
	 *
	 * So if we have already initialized all the widgets of a pipeline
	 * we skip, so check for list_empty and create the list if empty
	 */
	if (list_empty(&s_pipe->w_list)) {
		ret = skl_tplg_alloc_pipe_widget(ctx->dev, w, s_pipe);
		if (ret < 0)
			return ret;
	}

	/* Init all pipe modules from source to sink */
	ret = skl_tplg_init_pipe_modules(skl, s_pipe);
	if (ret < 0)
		return ret;

	/* Bind modules from source to sink */
	list_for_each_entry(w_module, &s_pipe->w_list, node) {
		dst_module = w_module->w->priv;

		if (src_module == NULL) {
			src_module = dst_module;
			continue;
		}

		ret = skl_bind_modules(ctx, src_module, dst_module);
		if (ret < 0)
			return ret;

		src_module = dst_module;
	}

	skl_tplg_alloc_pipe_mem(skl, mconfig);
	skl_tplg_alloc_pipe_mcps(skl, mconfig);

	return 0;
}

static int skl_tplg_bind_sinks(struct snd_soc_dapm_widget *w,
				struct skl *skl,
				struct snd_soc_dapm_widget *src_w,
				struct skl_module_cfg *src_mconfig)
{
	struct snd_soc_dapm_path *p;
	struct snd_soc_dapm_widget *sink = NULL, *next_sink = NULL;
	struct skl_module_cfg *sink_mconfig;
	struct skl_sst *ctx = skl->skl_sst;
	int ret;

	snd_soc_dapm_widget_for_each_sink_path(w, p) {
		if (!p->connect)
			continue;

		dev_dbg(ctx->dev, "%s: src widget=%s\n", __func__, w->name);
		dev_dbg(ctx->dev, "%s: sink widget=%s\n", __func__, p->sink->name);

		next_sink = p->sink;

		if (!is_skl_dsp_widget_type(p->sink))
			return skl_tplg_bind_sinks(p->sink, skl, src_w, src_mconfig);

		/*
		 * here we will check widgets in sink pipelines, so that
		 * can be any widgets type and we are only interested if
		 * they are ones used for SKL so check that first
		 */
		if ((p->sink->priv != NULL) && is_skl_dsp_widget_type(p->sink)) {
			sink = p->sink;
			sink_mconfig = sink->priv;

			/* Bind source to sink, mixin is always source */
			ret = skl_bind_modules(ctx, src_mconfig, sink_mconfig);
			if (ret)
				return ret;

			/* Start sinks pipe first */
			if (sink_mconfig->pipe->state != SKL_PIPE_STARTED) {
				if (sink_mconfig->pipe->conn_type != SKL_PIPE_CONN_TYPE_FE)
					ret = skl_run_pipe(ctx, sink_mconfig->pipe);
				if (ret)
					return ret;
			}
		}
	}

	if ((!sink) && (next_sink))
		return skl_tplg_bind_sinks(next_sink, skl, src_w, src_mconfig);

	return 0;
}

/*
 * A PGA represents a module in a pipeline. So in the Pre-PMU event of PGA
 * we need to do following:
 *   - Bind to sink pipeline
 *   	Since the sink pipes can be running and we don't get mixer event on
 *   	connect for already running mixer, we need to find the sink pipes
 *   	here and bind to them. This way dynamic connect works.
 *   - Start sink pipeline, if not running
 *   - Then run current pipe
 */
static int skl_tplg_pga_dapm_pre_pmu_event(struct snd_soc_dapm_widget *w,
								struct skl *skl)
{
	struct skl_module_cfg *src_mconfig;
	struct skl_sst *ctx = skl->skl_sst;
	int ret = 0;

	dev_dbg(ctx->dev, "%s: widget =%s\n", __func__, w->name);

	src_mconfig = w->priv;

	/*
	 * find which sink it is connected to, bind with the sink,
	 * if sink is not started, start sink pipe first, then start
	 * this pipe
	 */
	ret = skl_tplg_bind_sinks(w, skl, w, src_mconfig);
	if (ret)
		return ret;

	/* Start source pipe last after starting all sinks */
	if (src_mconfig->pipe->conn_type != SKL_PIPE_CONN_TYPE_FE)
		return skl_run_pipe(ctx, src_mconfig->pipe);

	return 0;
}

static struct snd_soc_dapm_widget *skl_get_src_dsp_widget(
		struct snd_soc_dapm_widget *w, struct skl *skl)
{
	struct snd_soc_dapm_path *p;
	struct snd_soc_dapm_widget *src_w = NULL;
	struct skl_sst *ctx = skl->skl_sst;

	snd_soc_dapm_widget_for_each_source_path(w,p) {
		src_w = p->source;
		if (!p->connect)
			continue;
		dev_dbg(ctx->dev, "sink widget=%s\n", w->name);
		dev_dbg(ctx->dev, "src widget=%s\n", p->source->name);

		/*
		 * here we will check widgets in sink pipelines, so that can
		 * be any widgets type and we are only interested if they are
		 * ones used for SKL so check that first
		 */
		if ((p->source->priv != NULL) && is_skl_dsp_widget_type(p->source)) {
			return p->source;
		}
	}

	if (src_w != NULL)
		return skl_get_src_dsp_widget(src_w, skl);

	return NULL;
}

/*
 * in the Post-PMU event of mixer we need to do following:
 *   - Check if this pipe is running
 *   - if not, then
 *	- bind this pipeline to its source pipeline
 *	  if source pipe is already running, this means it is a dynamic
 *	  connection and we need to bind only to that pipe
 *	- start this pipeline
 */
static int skl_tplg_mixer_dapm_post_pmu_event(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	int ret = 0;
	struct snd_soc_dapm_widget *source, *sink;
	struct skl_module_cfg *src_mconfig, *sink_mconfig;
	struct skl_sst *ctx = skl->skl_sst;
	int src_pipe_started = 0;

	sink = w;
	sink_mconfig = sink->priv;

	/*
	 * If source pipe is already started, that means source is driving
	 * one more sink before this sink got connected, Since source is
	 * started, bind this sink to source and start this pipe.
	 */
	source = skl_get_src_dsp_widget(w, skl);
	if (source != NULL) {
		src_mconfig = source->priv;
		sink_mconfig = sink->priv;
		src_pipe_started = 1;

		/*
		 * check pipe state, then no need to bind or start the
		 * pipe
		 */
		if (src_mconfig->pipe->state != SKL_PIPE_STARTED)
			src_pipe_started = 0;
	}

	if (src_pipe_started) {
		ret = skl_bind_modules(ctx, src_mconfig, sink_mconfig);
		if (ret)
			return ret;

		if (sink_mconfig->pipe->conn_type != SKL_PIPE_CONN_TYPE_FE)
			ret = skl_run_pipe(ctx, sink_mconfig->pipe);
	}

	return ret;
}

/*
 * in the Pre-PMD event of mixer we need to do following:
 *   - Stop the pipe
 *   - find the source connections and remove that from dapm_path_list
 *   - unbind with source pipelines if still connected
 */
static int skl_tplg_mixer_dapm_pre_pmd_event(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	struct skl_module_cfg *src_mconfig, *sink_mconfig;
	int ret = 0, i;
	struct skl_sst *ctx = skl->skl_sst;

	sink_mconfig = w->priv;

	/* Stop the pipe */
	ret = skl_stop_pipe(ctx, sink_mconfig->pipe);
	if (ret)
		return ret;

	for (i = 0; i < sink_mconfig->max_in_queue; i++) {
		if (sink_mconfig->m_in_pin[i].pin_state == SKL_PIN_BIND_DONE) {
			src_mconfig = sink_mconfig->m_in_pin[i].tgt_mcfg;
			if (!src_mconfig)
				continue;

			ret = skl_unbind_modules(ctx,
						src_mconfig, sink_mconfig);
		}
	}

	return ret;
}

/*
 * in the Post-PMD event of mixer we need to do following:
 *   - Free the mcps used
 *   - Free the mem used
 *   - Unbind the modules within the pipeline
 *   - Delete the pipeline (modules are not required to be explicitly
 *     deleted, pipeline delete is enough here
 */
static int skl_tplg_mixer_dapm_post_pmd_event(struct snd_soc_dapm_widget *w,
							struct skl *skl)
{
	struct skl_module_cfg *mconfig = w->priv;
	struct skl_pipe_module *w_module;
	struct skl_module_cfg *src_module = NULL, *dst_module;
	struct skl_sst *ctx = skl->skl_sst;
	struct skl_pipe *s_pipe = mconfig->pipe;
	int ret = 0;

	skl_tplg_free_pipe_mcps(skl, mconfig);
	skl_tplg_free_pipe_mem(skl, mconfig);

	list_for_each_entry(w_module, &s_pipe->w_list, node) {
		dst_module = w_module->w->priv;

		skl_tplg_free_pipe_mcps(skl, dst_module);
		if (src_module == NULL) {
			src_module = dst_module;
			continue;
		}

		skl_unbind_modules(ctx, src_module, dst_module);
		src_module = dst_module;
	}

	ret = skl_delete_pipe(ctx, mconfig->pipe);

	return skl_tplg_unload_pipe_modules(ctx, s_pipe);
}

/*
 * in the Post-PMD event of PGA we need to do following:
 *   - Free the mcps used
 *   - Stop the pipeline
 *   - In source pipe is connected, unbind with source pipelines
 */
static int skl_tplg_pga_dapm_post_pmd_event(struct snd_soc_dapm_widget *w,
								struct skl *skl)
{
	struct skl_module_cfg *src_mconfig, *sink_mconfig;
	int ret = 0, i;
	struct skl_sst *ctx = skl->skl_sst;

	src_mconfig = w->priv;

	/* Stop the pipe since this is a mixin module */
	ret = skl_stop_pipe(ctx, src_mconfig->pipe);
	if (ret)
		return ret;

	for (i = 0; i < src_mconfig->max_out_queue; i++) {
		if (src_mconfig->m_out_pin[i].pin_state == SKL_PIN_BIND_DONE) {
			sink_mconfig = src_mconfig->m_out_pin[i].tgt_mcfg;
			if (!sink_mconfig)
				continue;
			/*
			 * This is a connecter and if path is found that means
			 * unbind between source and sink has not happened yet
			 */
			ret = skl_unbind_modules(ctx, src_mconfig,
							sink_mconfig);
		}
	}

	return ret;
}

/*
 * In modelling, we assume there will be ONLY one mixer in a pipeline.  If
 * mixer is not required then it is treated as static mixer aka vmixer with
 * a hard path to source module
 * So we don't need to check if source is started or not as hard path puts
 * dependency on each other
 */
static int skl_tplg_vmixer_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct skl *skl = get_skl_ctx(dapm->dev);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return skl_tplg_mixer_dapm_pre_pmu_event(w, skl);

	case SND_SOC_DAPM_POST_PMU:
		return skl_tplg_mixer_dapm_post_pmu_event(w, skl);

	case SND_SOC_DAPM_PRE_PMD:
		return skl_tplg_mixer_dapm_pre_pmd_event(w, skl);

	case SND_SOC_DAPM_POST_PMD:
		return skl_tplg_mixer_dapm_post_pmd_event(w, skl);
	}

	return 0;
}

/*
 * In modelling, we assume there will be ONLY one mixer in a pipeline. If a
 * second one is required that is created as another pipe entity.
 * The mixer is responsible for pipe management and represent a pipeline
 * instance
 */
static int skl_tplg_mixer_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct skl *skl = get_skl_ctx(dapm->dev);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return skl_tplg_mixer_dapm_pre_pmu_event(w, skl);

	case SND_SOC_DAPM_POST_PMU:
		return skl_tplg_mixer_dapm_post_pmu_event(w, skl);

	case SND_SOC_DAPM_PRE_PMD:
		return skl_tplg_mixer_dapm_pre_pmd_event(w, skl);

	case SND_SOC_DAPM_POST_PMD:
		return skl_tplg_mixer_dapm_post_pmd_event(w, skl);
	}

	return 0;
}

/*
 * In modelling, we assumed rest of the modules in pipeline are PGA. But we
 * are interested in last PGA (leaf PGA) in a pipeline to disconnect with
 * the sink when it is running (two FE to one BE or one FE to two BE)
 * scenarios
 */
static int skl_tplg_pga_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *k, int event)

{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct skl *skl = get_skl_ctx(dapm->dev);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return skl_tplg_pga_dapm_pre_pmu_event(w, skl);

	case SND_SOC_DAPM_POST_PMD:
		return skl_tplg_pga_dapm_post_pmd_event(w, skl);
	}

	return 0;
}
static int skl_tplg_dsp_log_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata
					(&(platform->component));
	struct skl *skl = ebus_to_skl(ebus);

	ucontrol->value.integer.value[0] = get_dsp_log_priority(skl);

	return 0;
}

static int skl_tplg_dsp_log_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_platform *platform = snd_soc_kcontrol_platform(kcontrol);
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata
					(&(platform->component));
	struct skl *skl = ebus_to_skl(ebus);

	update_dsp_log_priority(ucontrol->value.integer.value[0], skl);

	return 0;
}


static int skl_tplg_send_gain_ipc(struct snd_soc_dapm_context *dapm,
					struct skl_module_cfg *mconfig)
{
	struct skl_gain_config *gain_cfg;
	struct skl *skl = get_skl_ctx(dapm->dev);
	int num_channel, i, ret = 0;

	num_channel = mconfig->out_fmt[0].channels;

	gain_cfg = kzalloc(sizeof(*gain_cfg), GFP_KERNEL);
	if (!gain_cfg)
		return -ENOMEM;

	gain_cfg->ramp_type = mconfig->gain_data.ramp_type;
	gain_cfg->ramp_duration = mconfig->gain_data.ramp_duration;
	for (i = 0; i < num_channel; i++) {
		gain_cfg->channel_id = i;
		gain_cfg->target_volume = mconfig->gain_data.volume[i];
		ret = skl_set_module_params(skl->skl_sst, (u32 *)gain_cfg,
					sizeof(*gain_cfg), 0, mconfig);
		if (ret < 0) {
			dev_err(dapm->dev, "Gain for channel:%d failed\n", i);
			break;
		}
	}
	kfree(gain_cfg);

	return ret;
}

static int skl_tplg_ramp_duration_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;

	ucontrol->value.integer.value[0] = mconfig->gain_data.ramp_duration;

	return 0;
}

static int skl_tplg_ramp_type_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;

	ucontrol->value.integer.value[0] = mconfig->gain_data.ramp_type;

	return 0;
}

static int skl_tplg_get_linear_toindex(int val)
{
	int i, index = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(linear_gain); i++) {
		if (val == linear_gain[i]) {
			index = i;
			break;
		}
	}

	return index;
}

static int skl_tplg_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;

	ucontrol->value.integer.value[0] = skl_tplg_get_linear_toindex(mconfig->gain_data.volume[0]);
	ucontrol->value.integer.value[1] = skl_tplg_get_linear_toindex(mconfig->gain_data.volume[1]);

	return 0;
}

static int skl_tplg_ramp_duration_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	int ret = 0;

	mconfig->gain_data.ramp_duration = ucontrol->value.integer.value[0];

	if (w->power)
		ret = skl_tplg_send_gain_ipc(dapm, mconfig);
	return ret;
}

static int skl_tplg_ramp_type_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	int ret = 0;

	mconfig->gain_data.ramp_type = ucontrol->value.integer.value[0];

	if (w->power)
		ret = skl_tplg_send_gain_ipc(dapm, mconfig);

	return ret;
}

static int skl_tplg_volume_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	int ret = 0;

	if ((ucontrol->value.integer.value[0] >= ARRAY_SIZE(linear_gain)) ||
		(ucontrol->value.integer.value[1] >= ARRAY_SIZE(linear_gain))) {
		dev_err(dapm->dev, "Volume requested is out of range!!!\n");
		return -EINVAL;
	}
	mconfig->gain_data.volume[0] = linear_gain[ucontrol->value.integer.value[0]];
	mconfig->gain_data.volume[1] = linear_gain[ucontrol->value.integer.value[1]];

	if (w->power)
		ret = skl_tplg_send_gain_ipc(dapm, mconfig);

	return ret;
}

static int skl_tplg_tlv_control_get(struct snd_kcontrol *kcontrol,
			unsigned int __user *data, unsigned int size)
{
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct soc_bytes_ext *sb = (void *) kcontrol->private_value;
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct skl_algo_data *bc = (struct skl_algo_data *)sb->dobj.private;
	struct skl *skl = get_skl_ctx(dapm->dev);
	struct skl_notify_data *notify_data;
	int ret = 0;

	dev_dbg(dapm->dev, "In%s control_name=%s, id=%u\n", __func__, kcontrol->id.name, bc->param_id);
	dev_dbg(dapm->dev, "size = %u (%#x), max = %#x\n", size, size, bc->max);

	if (w->power)
		skl_get_module_params(skl->skl_sst, (void *)bc->params,
				      bc->max, bc->param_id, mconfig);

	/* decrement size for TLV header */
	size -= 2 * sizeof(u32);

	/* check size as we don't want to send kernel data */
	if (size > bc->max)
		size = bc->max;

	if (bc->params) {
		ret = copy_to_user(data, &bc->param_id, sizeof(u32));
		ret = copy_to_user(((unsigned char *)data) + sizeof(u32), &bc->max, sizeof(u32));
		ret = copy_to_user(((unsigned char *)data) + 2*sizeof(u32), bc->params, size);

	}

	/* Notification payload will be set to 0 after read is done */
	if (bc->notification_ctrl) {
		notify_data = (struct skl_notify_data *)bc->params;

		if (notify_data != NULL)
			memset(notify_data->data, 0, notify_data->length);
	}

	return  ret;
}

static int skl_tplg_tlv_control_set(struct snd_kcontrol *kcontrol,
			const unsigned int __user *data, unsigned int size)
{
	struct snd_soc_dapm_context *dapm =
				snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct soc_bytes_ext *sb = (void *) kcontrol->private_value;
	struct skl_algo_data *ac = (struct skl_algo_data *)sb->dobj.private;
	struct skl *skl = get_skl_ctx(dapm->dev);

	dev_dbg(dapm->dev, "in %s control=%s\n", __func__, kcontrol->id.name);
	dev_dbg(dapm->dev, "size = %u, %#x\n", size, size);

	if (ac->params && (ac->access_type == SKL_WIDGET_WRITE ||
				ac->access_type == SKL_WIDGET_READ_WRITE)) {
		memset(ac->params, 0, ac->max);
                /* skip copying first two u32 words from user which is the TLV header */
		if (copy_from_user(ac->params,
				   ((unsigned char *)data) + 2*sizeof(u32),
				   size - 2*sizeof(u32)))
			return -EIO;

		if (w->power)
			return skl_set_module_params(skl->skl_sst, (void *)ac->params,
						ac->max, ac->param_id, mconfig);
	}
	return 0;
}
static int skl_cache_probe_param(struct snd_kcontrol *kctl,
			struct skl_probe_data *ap, struct skl_sst *ctx)
{
	struct skl_probe_config *pconfig = &ctx->probe_config;
	union skl_connector_node_id node_id = {-1};
	int index = -1, i;
	char buf[20], pos[10];

	if (ap->is_ext_inj == SKL_PROBE_EXTRACT) {
		/* From the control ID get the extractor index */
		for (i = 0; i < pconfig->no_extractor; i++) {
			strcpy(buf, "Extractor");
			snprintf(pos, 4, "%d", i);
			if (strstr(kctl->id.name, strcat(buf, pos))) {
				index = i;
				break;
			}
		}

		if (index < 0)
			return -EINVAL;

		pr_debug("Setting extractor probe index %d\n", index);
		memcpy(&ap->node_id, &node_id, sizeof(u32));
		pconfig->eprobe[index].id = ap->params;
		if (ap->is_connect == SKL_PROBE_CONNECT)
			pconfig->eprobe[index].set = 1;
		else if (ap->is_connect == SKL_PROBE_DISCONNECT)
			pconfig->eprobe[index].set = -1;

	} else {
		/* From the control ID get the injector index */
		for (i = 0; i < pconfig->no_injector; i++) {
			strcpy(buf, "Injector");
			snprintf(pos, 4, "%d", i);
			if (strstr(kctl->id.name, strcat(buf, pos))) {
				index = i;
				break;
			}
		}

		if (index < 0)
			return -EINVAL;

		pconfig->iprobe[index].id = ap->params;
		node_id.node.dma_type = SKL_DMA_HDA_HOST_OUTPUT_CLASS;
		node_id.node.vindex = pconfig->iprobe[index].dma_id;
		memcpy(&ap->node_id, &node_id, sizeof(u32));
		if (ap->is_connect == SKL_PROBE_CONNECT)
			pconfig->eprobe[index].set = 1;
		else if (ap->is_connect == SKL_PROBE_DISCONNECT)
			pconfig->eprobe[index].set = -1;
	}
	return 0;
}

static int skl_tplg_tlv_probe_set(struct snd_kcontrol *kcontrol,
			const unsigned int __user *data, unsigned int size)
{
	struct snd_soc_dapm_context *dapm =
				snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_dapm_widget *w = snd_soc_dapm_kcontrol_widget(kcontrol);
	struct skl_module_cfg *mconfig = w->priv;
	struct soc_bytes_ext *sb = (void *) kcontrol->private_value;
	struct skl_probe_data *ap = (struct skl_probe_data *)sb->dobj.private;
	struct skl *skl = get_skl_ctx(dapm->dev);
	struct skl_probe_config *pconfig = &skl->skl_sst->probe_config;
	struct probe_pt_param connect_point;
	int disconnect_point;
	void *offset;
	int ret;

	dev_dbg(dapm->dev, "in %s control=%s\n", __func__, kcontrol->id.name);
	dev_dbg(dapm->dev, "size = %u, %#x\n", size, size);

	if (data) {
		offset = (unsigned char *)data;
		offset += 2 * sizeof(u32); /* To skip TLV heeader */
		if (copy_from_user(&ap->is_connect,
					offset, sizeof(ap->is_connect)))
			return -EIO;

		offset += sizeof(ap->is_connect);
		if (copy_from_user(&ap->is_ext_inj,
					offset, sizeof(ap->is_ext_inj)))
			return -EIO;

		offset += sizeof(ap->is_ext_inj);
		if (copy_from_user(&ap->params,
					offset, sizeof(ap->params)))
			return -EIO;

		dev_dbg(dapm->dev, "connect state = %d, extract_inject = %d, params = %d \n",
						ap->is_connect, ap->is_ext_inj, ap->params);

		ret = skl_cache_probe_param(kcontrol, ap, skl->skl_sst);
		if (ret < 0)
			return -EINVAL;

		if (pconfig->probe_count) {
			/* In the case of extraction, additional probe points can be set when
			 * the stream is in progress and the driver can immediately send the
			 * connect IPC. But in the case of injector, for each probe point
			 * connection a new stream with the DAI number corresponding to that
			 * control has to be opened. Hence below check ensures that the
			 * connect IPC is sent only in case of extractor.
			 */
			if ((ap->is_connect == SKL_PROBE_CONNECT)
				&& (ap->is_ext_inj == SKL_PROBE_EXTRACT)) {

				memcpy(&connect_point.params, &ap->params, sizeof(u32));
				connect_point.connection = ap->is_ext_inj;
				memcpy(&connect_point.node_id, (&ap->node_id), sizeof(u32));
				return skl_set_module_params(skl->skl_sst, (void *)&connect_point,
						sizeof(struct probe_pt_param), ap->is_connect, mconfig);

			} else if (ap->is_connect == SKL_PROBE_DISCONNECT) {

				disconnect_point = (int)ap->params;
				return skl_set_module_params(skl->skl_sst, (void *)&disconnect_point,
						sizeof(disconnect_point), ap->is_connect, mconfig);
			}
		}
	}
	return 0;
}

/*
 * The FE params are passed by hw_params of the DAI.
 * On hw_params, the params are stored in Gateway module of the FE and we
 * need to calculate the format in DSP module configuration, that
 * conversion is done here
 */
int skl_tplg_update_pipe_params(struct device *dev,
			struct skl_module_cfg *mconfig,
			struct skl_pipe_params *params,
			snd_pcm_format_t fmt)
{
	struct skl_pipe *pipe = mconfig->pipe;
	struct skl_module_fmt *format = NULL;

	memcpy(pipe->p_params, params, sizeof(*params));

	if (params->stream == SNDRV_PCM_STREAM_PLAYBACK)
		format = &mconfig->in_fmt[0];
	else
		format = &mconfig->out_fmt[0];

	/* set the hw_params */
	format->s_freq = params->s_freq;
	format->channels = params->ch;

	/*
		* set copier sample type as follows
		* 0 : if data is MSB aligned in the container
		* 1 : if data is LSB aligned in the container
		* 4 : if float
	*/
	switch (fmt) {
	case SKL_FMT_S16LE:
		format->valid_bit_depth = SKL_DEPTH_16BIT;
		format->bit_depth = SKL_DEPTH_16BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_MSB;
		break;

	case SKL_FMT_S24LE:
		format->valid_bit_depth = SKL_DEPTH_24BIT;
		format->bit_depth = SKL_DEPTH_32BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_LSB;
		break;

	case SKL_FMT_S32LE:
		format->valid_bit_depth = SKL_DEPTH_32BIT;
		format->bit_depth = SKL_DEPTH_32BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_MSB;
		break;

	case SKL_FMT_FLOATLE:
		format->valid_bit_depth = SKL_DEPTH_32BIT;
		format->bit_depth = SKL_DEPTH_32BIT;
		format->sample_type = SKL_SAMPLE_TYPE_FLOAT;
		break;

	case SKL_FMT_S24_3LE:
		format->valid_bit_depth = SKL_DEPTH_24BIT;
		format->bit_depth = SKL_DEPTH_24BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_MSB;
		break;

	default:
		format->bit_depth = SKL_DEPTH_32BIT;
		format->sample_type = SKL_SAMPLE_TYPE_INT_MSB;
	}

	/* take care of fractional rates, round to next integer */
	if (params->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mconfig->ibs = ((format->s_freq + 999) / 1000) *
				(format->channels) *
				(format->bit_depth >> 3);
	} else {
		mconfig->obs = ((format->s_freq + 999) / 1000) *
				(format->channels) *
				(format->bit_depth >> 3);
	}

	return 0;
}

/*
 * Query the module config for the FE DAI
 * This is used to find the hw_params set for that DAI and apply to FE
 * pipeline
 */
struct skl_module_cfg *
skl_tplg_fe_get_cpr_module(struct snd_soc_dai *dai, int stream)
{
	struct snd_soc_dapm_widget *w;
	struct snd_soc_dapm_path *p = NULL;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		w = dai->playback_widget;
		snd_soc_dapm_widget_for_each_sink_path(w, p) {
			if (p->connect && p->sink->power &&
					!is_skl_dsp_widget_type(p->sink))
				continue;

			if (p->sink->priv) {
				dev_dbg(dai->dev, "set params for %s\n",
						p->sink->name);
				return p->sink->priv;
			}
		}
	} else {
		w = dai->capture_widget;
		snd_soc_dapm_widget_for_each_source_path(w, p) {
			if (p->connect && p->source->power &&
					!is_skl_dsp_widget_type(p->source))
				continue;

			if (p->source->priv) {
				dev_dbg(dai->dev, "set params for %s\n",
						p->source->name);
				return p->source->priv;
			}
		}
	}

	return NULL;
}

static u8 skl_tplg_be_link_type(int dev_type)
{
	int ret;

	switch (dev_type) {
	case SKL_DEVICE_BT:
		ret = NHLT_LINK_SSP;
		break;

	case SKL_DEVICE_DMIC:
		ret = NHLT_LINK_DMIC;
		break;

	case SKL_DEVICE_I2S:
		ret = NHLT_LINK_SSP;
		break;

	case SKL_DEVICE_HDALINK:
		ret = NHLT_LINK_HDA;
		break;

	default:
		ret = NHLT_LINK_INVALID;
		break;
	}

	return ret;
}

/*
 * Fill the BE gateway parameters
 * The BE gateway expects a blob of parameters which are kept in the ACPI
 * NHLT blob, so query the blob for interface type (i2s/pdm) and instance.
 * The port can have multiple settings so pick based on the PCM
 * parameters
 */
static int skl_tplg_be_fill_pipe_params(struct snd_soc_dai *dai,
				struct skl_module_cfg *mconfig,
				struct skl_pipe_params *params)
{
	struct skl_pipe *pipe = mconfig->pipe;
	struct nhlt_specific_cfg *cfg;
	struct skl *skl = get_skl_ctx(dai->dev);
	int link_type = skl_tplg_be_link_type(mconfig->dev_type);

	memcpy(pipe->p_params, params, sizeof(*params));

	if (link_type == NHLT_LINK_HDA)
		return 0;

	/* update the blob based on virtual bus_id*/
	if (!skl->nhlt_override) {
		cfg = skl_get_ep_blob(skl, mconfig->vbus_id, link_type,
					params->s_fmt, params->ch,
					params->s_freq, params->stream);
	} else {
		dev_warn(dai->dev, "Querying NHLT blob from Debugfs!!!!\n");
		cfg = skl_nhlt_get_debugfs_blob(skl->debugfs,
					link_type, mconfig->vbus_id,
					params->stream);
	}
	if (cfg) {
		mconfig->formats_config.caps_size = cfg->size;
		mconfig->formats_config.caps = (u32 *) &cfg->caps;
	} else {
		dev_err(dai->dev, "Blob NULL for id %x type %d dirn %d\n",
					mconfig->vbus_id, link_type,
					params->stream);
		dev_err(dai->dev, "PCM: ch %d, freq %d, fmt %d\n",
				 params->ch, params->s_freq, params->s_fmt);
		return -EINVAL;
	}

	return 0;
}

static int skl_tplg_be_set_src_pipe_params(struct snd_soc_dai *dai,
				struct snd_soc_dapm_widget *w,
				struct skl_pipe_params *params)
{
	struct snd_soc_dapm_path *p;
	int ret = -EIO;

	snd_soc_dapm_widget_for_each_source_path(w, p) {
		if (p->connect && is_skl_dsp_widget_type(p->source) &&
						p->source->priv) {

			ret = skl_tplg_be_fill_pipe_params(
					dai, p->source->priv,
					params);
			if (ret < 0)
				return ret;
		} else {
			ret = skl_tplg_be_set_src_pipe_params(
						dai, p->source,	params);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int skl_tplg_be_set_sink_pipe_params(struct snd_soc_dai *dai,
	struct snd_soc_dapm_widget *w, struct skl_pipe_params *params)
{
	struct snd_soc_dapm_path *p = NULL;
	int ret = -EIO;

	snd_soc_dapm_widget_for_each_sink_path(w, p) {
		if (p->connect && is_skl_dsp_widget_type(p->sink) &&
						p->sink->priv) {

			ret = skl_tplg_be_fill_pipe_params(
					dai, p->sink->priv, params);
			if (ret < 0)
				return ret;
		} else {
			ret = skl_tplg_be_set_sink_pipe_params(
						dai, p->sink, params);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

/*
 * BE hw_params can be a source parameters (capture) or sink parameters
 * (playback). Based on sink and source we need to either find the source
 * list or the sink list and set the pipeline parameters
 */
int skl_tplg_be_update_params(struct snd_soc_dai *dai,
				struct skl_pipe_params *params)
{
	struct snd_soc_dapm_widget *w;

	if (params->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		w = dai->playback_widget;

		return skl_tplg_be_set_src_pipe_params(dai, w, params);

	} else {
		w = dai->capture_widget;

		return skl_tplg_be_set_sink_pipe_params(dai, w, params);
	}

	return 0;
}

static const struct snd_soc_tplg_widget_events skl_tplg_widget_ops[] = {
	{SKL_MIXER_EVENT, skl_tplg_mixer_event},
	{SKL_VMIXER_EVENT, skl_tplg_vmixer_event},
	{SKL_PGA_EVENT, skl_tplg_pga_event},
};

const struct snd_soc_tplg_bytes_ext_ops skl_tlv_ops[] = {
	{SKL_CONTROL_TYPE_BYTE_EXT, skl_tplg_tlv_control_get,
					skl_tplg_tlv_control_set},
	{SKL_CONTROL_TYPE_BYTE_PROBE, skl_tplg_tlv_control_get,
					skl_tplg_tlv_probe_set},
};
static const struct snd_soc_tplg_kcontrol_ops skl_tplg_kcontrol_ops[] = {
	{SKL_CONTROL_TYPE_VOLUME, skl_tplg_volume_get,
					skl_tplg_volume_set},
	{SKL_CONTROL_TYPE_RAMP_DURATION, skl_tplg_ramp_duration_get,
					skl_tplg_ramp_duration_set},
	{SKL_CONTROL_TYPE_RAMP_TYPE, skl_tplg_ramp_type_get,
					skl_tplg_ramp_type_set},
	{SKL_CONTROL_TYPE_DSP_LOG, skl_tplg_dsp_log_get,
					skl_tplg_dsp_log_set},
};

/*
 * The topology binary passes the pin info for a module so initialize the pin
 * info passed into module instance
 */
static void skl_fill_module_pin_info(struct skl_dfw_module_pin *dfw_pin,
						struct skl_module_pin *m_pin,
						bool is_dynamic, int max_pin)
{
	int i;

	for (i = 0; i < max_pin; i++) {
		m_pin[i].id.module_id = dfw_pin[i].module_id;
		m_pin[i].id.instance_id = dfw_pin[i].instance_id;
		m_pin[i].in_use = false;
		m_pin[i].is_dynamic = is_dynamic;
		m_pin[i].pin_state = SKL_PIN_UNBIND;
	}
}

/*
 * Add pipeline from topology binary into driver pipeline list
 *
 * If already added we return that instance
 * Otherwise we create a new instance and add into driver list
 */
static struct skl_pipe *skl_tplg_add_pipe(struct device *dev,
			struct skl *skl, struct skl_dfw_pipe *dfw_pipe)
{
	struct skl_pipeline *ppl;
	struct skl_pipe *pipe;
	struct skl_pipe_params *params;

	list_for_each_entry(ppl, &skl->ppl_list, node) {
		if (ppl->pipe->ppl_id == dfw_pipe->pipe_id)
			return ppl->pipe;
	}

	ppl = devm_kzalloc(dev, sizeof(*ppl), GFP_KERNEL);
	if (!ppl)
		return NULL;

	pipe = devm_kzalloc(dev, sizeof(*pipe), GFP_KERNEL);
	if (!pipe)
		return NULL;

	params = devm_kzalloc(dev, sizeof(*params), GFP_KERNEL);
	if (!params)
		return NULL;

	pipe->ppl_id = dfw_pipe->pipe_id;
	pipe->memory_pages = dfw_pipe->memory_pages;
	pipe->pipe_priority = dfw_pipe->pipe_priority;
	pipe->conn_type = dfw_pipe->conn_type;
	pipe->lp_mode = dfw_pipe->lp_mode;
	pipe->state = SKL_PIPE_INVALID;
	pipe->p_params = params;
	INIT_LIST_HEAD(&pipe->w_list);

	ppl->pipe = pipe;
	list_add(&ppl->node, &skl->ppl_list);

	return ppl->pipe;
}

static void skl_tplg_fill_fmt(struct skl_module_fmt *dst_fmt,
				struct skl_dfw_module_fmt *src_fmt,
				int pins)
{
	int i;

	for (i = 0; i < pins; i++) {
		dst_fmt[i].channels  = src_fmt[i].channels;
		dst_fmt[i].s_freq = src_fmt[i].freq;
		dst_fmt[i].bit_depth = src_fmt[i].bit_depth;
		dst_fmt[i].valid_bit_depth = src_fmt[i].valid_bit_depth;
		dst_fmt[i].ch_cfg = src_fmt[i].ch_cfg;
		dst_fmt[i].ch_map = src_fmt[i].ch_map;
		dst_fmt[i].interleaving_style = src_fmt[i].interleaving_style;
		dst_fmt[i].sample_type = src_fmt[i].sample_type;
	}
}

#define SKL_CURVE_NONE 0
#define SKL_MAX_GAIN 0x7FFFFFFF

static void skl_load_widget_params(struct skl_dfw_module *dfw_config,
				struct skl_module_cfg *mconfig)
{
	switch(dfw_config->module_type) {
	case SKL_MODULE_TYPE_GAIN:
		mconfig->gain_data.ramp_duration = 0;
		mconfig->gain_data.ramp_type = SKL_CURVE_NONE;
		mconfig->gain_data.volume[0] = SKL_MAX_GAIN;
		mconfig->gain_data.volume[1] = SKL_MAX_GAIN;
		return;
	default:
		return;
	}
}
/*
 * Topology core widget load callback
 *
 * This is used to save the private data for each widget which gives
 * information to the driver about module and pipeline parameters which DSP
 * FW expects like ids, resource values, formats etc
 */
static int skl_tplg_widget_load(struct snd_soc_component *cmpnt,
				struct snd_soc_dapm_widget *w,
				struct snd_soc_tplg_dapm_widget *tplg_w)
{
	int ret;
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata(cmpnt);
	struct skl *skl = ebus_to_skl(ebus);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct skl_module_cfg *mconfig;
	struct skl_pipe *pipe;
	struct skl_dfw_module *dfw_config =
				(struct skl_dfw_module *)tplg_w->priv.data;

	if (!tplg_w->priv.size)
		goto bind_event;

	mconfig = devm_kzalloc(bus->dev, sizeof(*mconfig), GFP_KERNEL);

	if (!mconfig)
		return -ENOMEM;

	w->priv = mconfig;
	mconfig->id.module_id = dfw_config->module_id;
	skl_load_widget_params(dfw_config, mconfig);

	mconfig->id.instance_id = dfw_config->instance_id;
	mconfig->mcps = dfw_config->max_mcps;
	mconfig->ibs = dfw_config->ibs;
	mconfig->obs = dfw_config->obs;
	mconfig->core_id = dfw_config->core_id;
	mconfig->max_in_queue = dfw_config->max_in_queue;
	mconfig->max_out_queue = dfw_config->max_out_queue;
	mconfig->is_loadable = dfw_config->is_loadable;
	mconfig->domain = dfw_config->proc_domain;

	skl_tplg_fill_fmt(mconfig->in_fmt, dfw_config->in_fmt,
						MODULE_MAX_IN_PINS);
	skl_tplg_fill_fmt(mconfig->out_fmt, dfw_config->out_fmt,
						MODULE_MAX_OUT_PINS);

	mconfig->params_fixup = dfw_config->params_fixup;
	mconfig->converter = dfw_config->converter;
	mconfig->m_type = dfw_config->module_type;
	mconfig->vbus_id = dfw_config->vbus_id;
	mconfig->mem_pages = dfw_config->mem_pages;
	mconfig->fast_mode = dfw_config->fast_mode;
	mconfig->in_frame_size = dfw_config->in_frame_size;
	mconfig->out_frame_size = dfw_config->out_frame_size;
	mconfig->dma_buffer_size = dfw_config->dma_buffer_size;

	pipe = skl_tplg_add_pipe(bus->dev, skl, &dfw_config->pipe);
	if (pipe)
		mconfig->pipe = pipe;

	mconfig->dev_type = dfw_config->dev_type;
	mconfig->hw_conn_type = dfw_config->hw_conn_type;
	mconfig->time_slot = dfw_config->time_slot;
	mconfig->formats_config.caps_size = dfw_config->caps.caps_size;

	if (dfw_config->is_loadable)
		memcpy(mconfig->guid, dfw_config->uuid,
					ARRAY_SIZE(dfw_config->uuid));

	mconfig->m_in_pin = devm_kzalloc(bus->dev, (mconfig->max_in_queue) *
						sizeof(*mconfig->m_in_pin),
						GFP_KERNEL);
	if (!mconfig->m_in_pin)
		return -ENOMEM;

	mconfig->m_out_pin = devm_kzalloc(bus->dev, (mconfig->max_out_queue) *
						sizeof(*mconfig->m_out_pin),
						GFP_KERNEL);
	if (!mconfig->m_out_pin)
		return -ENOMEM;

	skl_fill_module_pin_info(dfw_config->in_pin, mconfig->m_in_pin,
						dfw_config->is_dynamic_in_pin,
						mconfig->max_in_queue);

	skl_fill_module_pin_info(dfw_config->out_pin, mconfig->m_out_pin,
						 dfw_config->is_dynamic_out_pin,
							mconfig->max_out_queue);

	if (mconfig->formats_config.caps_size == 0)
		goto bind_event;

	mconfig->formats_config.caps = (u32 *)devm_kzalloc(bus->dev,
			mconfig->formats_config.caps_size, GFP_KERNEL);

	if (mconfig->formats_config.caps == NULL)
		return -ENOMEM;

	memcpy(mconfig->formats_config.caps, dfw_config->caps.caps,
						 dfw_config->caps.caps_size);
	mconfig->formats_config.param_id = dfw_config->caps.param_id;
	mconfig->formats_config.set_params = dfw_config->caps.set_params;

	skl_debug_init_module(skl->debugfs, w, mconfig);

bind_event:
	if (tplg_w->event_type == 0) {
		dev_dbg(bus->dev, "ASoC: No event handler required\n");
		return 0;
	}

	ret = snd_soc_tplg_widget_bind_event(w, skl_tplg_widget_ops,
					ARRAY_SIZE(skl_tplg_widget_ops),
					tplg_w->event_type);

	if (ret) {
		dev_err(bus->dev, "%s: No matching event handlers found for %d\n",
					__func__, tplg_w->event_type);
		return -EINVAL;
	}

	return 0;
}

static int skl_init_algo_data(struct device *dev, struct soc_bytes_ext *be,
					struct snd_soc_tplg_bytes_control *bc)
{
	struct skl_algo_data *ac;
	struct skl_dfw_algo_data *dfw_ac =
				(struct skl_dfw_algo_data *)bc->priv.data;

	ac = devm_kzalloc(dev, sizeof(*ac), GFP_KERNEL);
	if (!ac)
		return -ENOMEM;

	/* Fill private data */
	ac->max = dfw_ac->max;
	ac->param_id = dfw_ac->param_id;
	ac->set_params = dfw_ac->set_params;
	ac->access_type = dfw_ac->access_type;
	ac->value_cacheable = dfw_ac->value_cacheable;
	ac->notification_ctrl = dfw_ac->notification_ctrl;


	if (ac->max) {
		ac->params = (char *) devm_kzalloc(dev, ac->max, GFP_KERNEL);
		if (!ac->params)
			return -ENOMEM;

		memcpy(ac->params, dfw_ac->params, ac->max);
	}

	be->dobj.private  = ac;
	return 0;
}

int skl_tplg_control_load(struct snd_soc_component *cmpnt,
				struct snd_kcontrol_new *kctl,
				struct snd_soc_tplg_ctl_hdr *hdr)
{
	struct soc_bytes_ext *sb;
	struct snd_soc_tplg_bytes_control *tplg_bc;
	struct hdac_ext_bus *ebus  = snd_soc_component_get_drvdata(cmpnt);
	struct hdac_bus *bus = ebus_to_hbus(ebus);

	switch (hdr->ops.info) {
	case SND_SOC_TPLG_CTL_BYTES:
		tplg_bc = container_of(hdr, struct snd_soc_tplg_bytes_control, hdr);
		if (kctl->access & SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK) {
			sb = (struct soc_bytes_ext *)kctl->private_value;
			if (tplg_bc->priv.size)
				return skl_init_algo_data(bus->dev, sb, tplg_bc);
		}
		break;

	default:
		dev_dbg(bus->dev, "Control load not supported %d:%d:%d\n",
			hdr->ops.get, hdr->ops.put, hdr->ops.info);
		break;

	}

	return 0;
}

static int skl_manifest_load(struct snd_soc_component *cmpnt,
				struct snd_soc_tplg_manifest *manifest)
{
	struct skl_dfw_manifest *minfo;
	struct hdac_ext_bus *ebus = snd_soc_component_get_drvdata(cmpnt);
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct skl *skl = ebus_to_skl(ebus);
	int ret = 0;
	u8 *mdata;

	minfo = &skl->skl_sst->manifest;
	mdata = manifest->priv.data;
	minfo->lib_count = *mdata;

	if (minfo->lib_count > HDA_MAX_LIB) {
		dev_err(bus->dev, "Exceeding max Library count. Got:%d\n",
				minfo->lib_count);
		ret = -EINVAL;
		goto exit_manifest;
	}

	mdata++;
	minfo->lib = devm_kzalloc(bus->dev, minfo->lib_count *
			(sizeof(struct lib_info)), GFP_KERNEL);

	if (!minfo->lib) {
		ret = -ENOMEM;
		goto exit_manifest;
	}

	memcpy(minfo->lib, mdata, (u32) (minfo->lib_count *
				(sizeof(struct lib_info))));

exit_manifest:
	return ret;
}


static struct snd_soc_tplg_ops skl_tplg_ops  = {
	.widget_load = skl_tplg_widget_load,
	.control_load = skl_tplg_control_load,
	.io_ops = skl_tplg_kcontrol_ops,
	.io_ops_count = ARRAY_SIZE(skl_tplg_kcontrol_ops),
	.bytes_ext_ops = skl_tlv_ops,
	.bytes_ext_ops_count = ARRAY_SIZE(skl_tlv_ops),
	.manifest = skl_manifest_load,
};

/* This will be read from topology manifest, currently defined here */
#define SKL_MAX_MCPS 30000000
#define SKL_FW_MAX_MEM 1000000

/*
 * SKL topology init routine
 */
int skl_tplg_init(struct snd_soc_platform *platform, struct hdac_ext_bus *ebus)
{
	int ret;
	struct hdac_bus *bus = ebus_to_hbus(ebus);
	struct skl *skl = ebus_to_skl(ebus);

	ret = request_firmware(&skl->tplg, "dfw_sst.bin", bus->dev);
	if (ret < 0) {
		dev_err(bus->dev, "tplg fw %s load failed with %d\n",
				"dfw_sst.bin", ret);
		return ret;
	}

	/*
	 * The complete tplg for SKL is loaded as index 0, we don't use
	 * any other index
	 */
	ret = snd_soc_tplg_component_load(&platform->component,
					&skl_tplg_ops, skl->tplg, 0);
	if (ret < 0) {
		dev_err(bus->dev, "tplg component load failed%d\n", ret);
		release_firmware(skl->tplg);
		return -EINVAL;
	}

	skl->resource.max_mcps = SKL_MAX_MCPS;
	skl->resource.max_mem = SKL_FW_MAX_MEM;


	return 0;
}
