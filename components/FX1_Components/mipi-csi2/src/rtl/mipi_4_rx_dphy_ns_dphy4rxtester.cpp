//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphy4rxtester
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_dphy4rxtester.h"

mipi_4_rx_dphy_ns_dphy4rxtester::mipi_4_rx_dphy_ns_dphy4rxtester(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(merge_testdout);
    sensitive << testdout_b[0] << testdout_b[1] << testdout_b[2] << testdout_b[3] << testdout_b[4] << testdout_b[5] << testdout_b[6] << testdout_b[7];
    SC_METHOD(assign_xaddr);
    sensitive << testcode_x << testcode;
    SC_METHOD(assign_testdout);
    sensitive << testdouts_0[0] << testdouts_0[1] << testdouts_0[2] << testdouts_0[3] << testdouts_0[4] << testdouts_0[5] << testdouts_0[6] << testdouts_0[7] << testdouts_0[8] << testdouts_0[9] << testdouts_0[10] << testdouts_0[11] << testdouts_0[12] << testdouts_0[13] << testdouts_0[14] << testdouts_0[15] << testdouts_0[16] << testdouts_0[17] << testdouts_0[18] << testdouts_0[19] << testdouts_0[20] << testdouts_0[21] << testdouts_0[22] << testdouts_0[23] << testdouts_0[24] << testdouts_0[25] << testdouts_0[26] << testdouts_0[27] << testdouts_0[28] << testdouts_0[29] << testdouts_0[30] << testdouts_0[31] << testdouts_0[32] << testdouts_0[33] << testdouts_0[34] << testdouts_0[35] << testdouts_0[36] << testdouts_0[37] << testdouts_0[38] << testdouts_0[39] << testdouts_0[40] << testdouts_0[41] << testdouts_0[42] << testdouts_0[43] << testdouts_0[44] << testdouts_0[45] << testdouts_0[46] << testdouts_0[47] << testdouts_0[48] << testdouts_0[49] << testdouts_0[50] << testdouts_0[51] << testdouts_0[52] << testdouts_0[53] << testdouts_0[54] << testdouts_0[55] << testdouts_0[56] << testdouts_0[57] << testdouts_0[58] << testdouts_0[59] << testdouts_0[60] << testdouts_0[61] << testdouts_0[62] << testdouts_0[63] << testdouts_0[64] << testdouts_0[65] << testdouts_0[66] << testdouts_0[67] << testdouts_0[68] << testdouts_0[69] << testdouts_0[70] << testdouts_0[71] << testdouts_0[72] << testdouts_0[73] << testdouts_0[74] << testdouts_0[75] << testdouts_0[76] << testdouts_0[77] << testdouts_0[78] << testdouts_0[79] << testdouts_0[80] << testdouts_0[81] << testdouts_0[82] << testdouts_0[83] << testdouts_0[84] << testdouts_0[85] << testdouts_0[86] << testdouts_0[87] << testdouts_0[88] << testdouts_0[89] << testdouts_0[90] << testdouts_0[91] << testdouts_0[92] << testdouts_0[93] << testdouts_0[94] << testdouts_0[95] << testdouts_0[96] << testdouts_0[97] << testdouts_0[98] << testdouts_0[99] << testdouts_0[100] << testdouts_0[101] << testdouts_0[102] << testdouts_0[103] << testdouts_0[104] << testdouts_0[105] << testdouts_0[106] << testdouts_0[107] << testdouts_0[108] << testdouts_0[109] << testdouts_0[110] << testdouts_0[111] << testdouts_0[112] << testdouts_0[113] << testdouts_0[114] << testdouts_0[115] << testdouts_0[116] << testdouts_0[117] << testdouts_0[118] << testdouts_0[119] << testdouts_0[120] << testdouts_0[121] << testdouts_0[122] << testdouts_0[123] << testdouts_0[124] << testdouts_0[125] << testdouts_0[126] << testdouts_0[127] << testdouts_0[128] << testdouts_0[129] << testdouts_0[130] << testdouts_0[131] << testdouts_0[132] << testdouts_0[133] << testdouts_0[134] << testdouts_0[135] << testdouts_0[136] << testdouts_0[137] << testdouts_0[138] << testdouts_0[139] << testdouts_0[140] << testdouts_0[141] << testdouts_0[142] << testdouts_0[143] << testdouts_0[144] << testdouts_0[145] << testdouts_0[146] << testdouts_0[147] << testdouts_0[148] << testdouts_0[149] << testdouts_0[150] << testdouts_0[151] << testdouts_0[152] << testdouts_0[153] << testdouts_0[154] << testdouts_0[155] << testdouts_0[156] << testdouts_0[157] << testdouts_0[158] << testdouts_0[159] << testdouts_0[160] << testdouts_0[161] << testdouts_0[162] << testdouts_0[163] << testdouts_0[164] << testdouts_0[165] << testdouts_0[166] << testdouts_0[167] << testdouts_0[168] << testdouts_0[169] << testdouts_0[170] << testdouts_0[171] << testdouts_0[172] << testdouts_0[173] << testdouts_0[174] << testdouts_0[175] << testdouts_0[176] << testdouts_0[177] << testdouts_0[178] << testdouts_0[179] << testdouts_0[180] << testdouts_0[181] << testdouts_0[182] << testdouts_0[183] << testdouts_0[184] << testdouts_0[185] << testdouts_0[186] << testdouts_0[187] << testdouts_0[188] << testdouts_0[189] << testdouts_0[190] << testdouts_0[191] << testdouts_0[192] << testdouts_0[193] << testdouts_0[194] << testdouts_0[195] << testdouts_0[196] << testdouts_0[197] << testdouts_0[198] << testdouts_0[199] << testdouts_0[200] << testdouts_0[201] << testdouts_0[202] << testdouts_0[203] << testdouts_0[204] << testdouts_0[205] << testdouts_0[206] << testdouts_0[207] << testdouts_0[208] << testdouts_0[209] << testdouts_0[210] << testdouts_0[211] << testdouts_0[212] << testdouts_0[213] << testdouts_0[214] << testdouts_0[215] << testdouts_0[216] << testdouts_0[217] << testdouts_0[218] << testdouts_0[219] << testdouts_0[220] << testdouts_0[221] << testdouts_0[222] << testdouts_0[223] << testdouts_0[224] << testdouts_0[225] << testdouts_0[226] << testdouts_0[227] << testdouts_0[228] << testdouts_0[229] << testdouts_0[230] << testdouts_0[231] << testdouts_0[232] << testdouts_0[233] << testdouts_0[234] << testdouts_0[235] << testdouts_0[236] << testdouts_0[237] << testdouts_0[238] << testdouts_0[239] << testdouts_0[240] << testdouts_0[241] << testdouts_0[242] << testdouts_0[243] << testdouts_0[244] << testdouts_0[245] << testdouts_0[246] << testdouts_0[247] << testdouts_0[248] << testdouts_0[249] << testdouts_0[250] << testdouts_0[251] << testdouts_0[252] << testdouts_0[253] << testdouts_0[254] << testdouts_0[255] << testdouts_0[256] << testdouts_0[257] << testdouts_0[258] << testdouts_0[259] << testdouts_0[260] << testdouts_0[261] << testdouts_0[262] << testdouts_0[263] << testdouts_0[264] << testdouts_0[265] << testdouts_0[266] << testdouts_0[267] << testdouts_0[268] << testdouts_0[269] << testdouts_0[270] << testdouts_0[271] << testdouts_0[272] << testdouts_0[273] << testdouts_0[274] << testdouts_0[275] << testdouts_0[276] << testdouts_0[277] << testdouts_0[278] << testdouts_0[279] << testdouts_0[280] << testdouts_0[281] << testdouts_0[282] << testdouts_0[283] << testdouts_0[284] << testdouts_0[285] << testdouts_0[286] << testdouts_0[287] << testdouts_0[288] << testdouts_0[289] << testdouts_0[290] << testdouts_0[291] << testdouts_0[292] << testdouts_0[293] << testdouts_0[294] << testdouts_0[295] << testdouts_0[296] << testdouts_0[297] << testdouts_0[298] << testdouts_0[299] << testdouts_0[300] << testdouts_0[301] << testdouts_0[302] << testdouts_0[303] << testdouts_0[304] << testdouts_0[305] << testdouts_0[306] << testdouts_0[307] << testdouts_0[308] << testdouts_0[309] << testdouts_0[310] << testdouts_0[311] << testdouts_0[312] << testdouts_0[313] << testdouts_0[314] << testcode_x << testcode;
    SC_METHOD(assign_testdout_2);
    sensitive << testdouts_1[0] << testdouts_1[1] << testdouts_1[2] << testdouts_1[3] << testdouts_1[4] << testdouts_1[5] << testdouts_1[6] << testdouts_1[7] << testdouts_1[8] << testdouts_1[9] << testdouts_1[10] << testdouts_1[11] << testdouts_1[12] << testdouts_1[13] << testdouts_1[14] << testdouts_1[15] << testdouts_1[16] << testdouts_1[17] << testdouts_1[18] << testdouts_1[19] << testdouts_1[20] << testdouts_1[21] << testdouts_1[22] << testdouts_1[23] << testdouts_1[24] << testdouts_1[25] << testdouts_1[26] << testdouts_1[27] << testdouts_1[28] << testdouts_1[29] << testdouts_1[30] << testdouts_1[31] << testdouts_1[32] << testdouts_1[33] << testdouts_1[34] << testdouts_1[35] << testdouts_1[36] << testdouts_1[37] << testdouts_1[38] << testdouts_1[39] << testdouts_1[40] << testdouts_1[41] << testdouts_1[42] << testdouts_1[43] << testdouts_1[44] << testdouts_1[45] << testdouts_1[46] << testdouts_1[47] << testdouts_1[48] << testdouts_1[49] << testdouts_1[50] << testdouts_1[51] << testdouts_1[52] << testdouts_1[53] << testdouts_1[54] << testdouts_1[55] << testdouts_1[56] << testdouts_1[57] << testdouts_1[58] << testdouts_1[59] << testdouts_1[60] << testdouts_1[61] << testdouts_1[62] << testdouts_1[63] << testdouts_1[64] << testdouts_1[65] << testdouts_1[66] << testdouts_1[67] << testdouts_1[68] << testdouts_1[69] << testdouts_1[70] << testdouts_1[71] << testdouts_1[72] << testdouts_1[73] << testdouts_1[74] << testdouts_1[75] << testdouts_1[76] << testdouts_1[77] << testdouts_1[78] << testdouts_1[79] << testdouts_1[80] << testdouts_1[81] << testdouts_1[82] << testdouts_1[83] << testdouts_1[84] << testdouts_1[85] << testdouts_1[86] << testdouts_1[87] << testdouts_1[88] << testdouts_1[89] << testdouts_1[90] << testdouts_1[91] << testdouts_1[92] << testdouts_1[93] << testdouts_1[94] << testdouts_1[95] << testdouts_1[96] << testdouts_1[97] << testdouts_1[98] << testdouts_1[99] << testdouts_1[100] << testdouts_1[101] << testdouts_1[102] << testdouts_1[103] << testdouts_1[104] << testdouts_1[105] << testdouts_1[106] << testdouts_1[107] << testdouts_1[108] << testdouts_1[109] << testdouts_1[110] << testdouts_1[111] << testdouts_1[112] << testdouts_1[113] << testdouts_1[114] << testdouts_1[115] << testdouts_1[116] << testdouts_1[117] << testdouts_1[118] << testdouts_1[119] << testdouts_1[120] << testdouts_1[121] << testdouts_1[122] << testdouts_1[123] << testdouts_1[124] << testdouts_1[125] << testdouts_1[126] << testdouts_1[127] << testdouts_1[128] << testdouts_1[129] << testdouts_1[130] << testdouts_1[131] << testdouts_1[132] << testdouts_1[133] << testdouts_1[134] << testdouts_1[135] << testdouts_1[136] << testdouts_1[137] << testdouts_1[138] << testdouts_1[139] << testdouts_1[140] << testdouts_1[141] << testdouts_1[142] << testdouts_1[143] << testdouts_1[144] << testdouts_1[145] << testdouts_1[146] << testdouts_1[147] << testdouts_1[148] << testdouts_1[149] << testdouts_1[150] << testdouts_1[151] << testdouts_1[152] << testdouts_1[153] << testdouts_1[154] << testdouts_1[155] << testdouts_1[156] << testdouts_1[157] << testdouts_1[158] << testdouts_1[159] << testdouts_1[160] << testdouts_1[161] << testdouts_1[162] << testdouts_1[163] << testdouts_1[164] << testdouts_1[165] << testdouts_1[166] << testdouts_1[167] << testdouts_1[168] << testdouts_1[169] << testdouts_1[170] << testdouts_1[171] << testdouts_1[172] << testdouts_1[173] << testdouts_1[174] << testdouts_1[175] << testdouts_1[176] << testdouts_1[177] << testdouts_1[178] << testdouts_1[179] << testdouts_1[180] << testdouts_1[181] << testdouts_1[182] << testdouts_1[183] << testdouts_1[184] << testdouts_1[185] << testdouts_1[186] << testdouts_1[187] << testdouts_1[188] << testdouts_1[189] << testdouts_1[190] << testdouts_1[191] << testdouts_1[192] << testdouts_1[193] << testdouts_1[194] << testdouts_1[195] << testdouts_1[196] << testdouts_1[197] << testdouts_1[198] << testdouts_1[199] << testdouts_1[200] << testdouts_1[201] << testdouts_1[202] << testdouts_1[203] << testdouts_1[204] << testdouts_1[205] << testdouts_1[206] << testdouts_1[207] << testdouts_1[208] << testdouts_1[209] << testdouts_1[210] << testdouts_1[211] << testdouts_1[212] << testdouts_1[213] << testdouts_1[214] << testdouts_1[215] << testdouts_1[216] << testdouts_1[217] << testdouts_1[218] << testdouts_1[219] << testdouts_1[220] << testdouts_1[221] << testdouts_1[222] << testdouts_1[223] << testdouts_1[224] << testdouts_1[225] << testdouts_1[226] << testdouts_1[227] << testdouts_1[228] << testdouts_1[229] << testdouts_1[230] << testdouts_1[231] << testdouts_1[232] << testdouts_1[233] << testdouts_1[234] << testdouts_1[235] << testdouts_1[236] << testdouts_1[237] << testdouts_1[238] << testdouts_1[239] << testdouts_1[240] << testdouts_1[241] << testdouts_1[242] << testdouts_1[243] << testdouts_1[244] << testdouts_1[245] << testdouts_1[246] << testdouts_1[247] << testdouts_1[248] << testdouts_1[249] << testdouts_1[250] << testdouts_1[251] << testdouts_1[252] << testdouts_1[253] << testdouts_1[254] << testdouts_1[255] << testdouts_1[256] << testdouts_1[257] << testdouts_1[258] << testdouts_1[259] << testdouts_1[260] << testdouts_1[261] << testdouts_1[262] << testdouts_1[263] << testdouts_1[264] << testdouts_1[265] << testdouts_1[266] << testdouts_1[267] << testdouts_1[268] << testdouts_1[269] << testdouts_1[270] << testdouts_1[271] << testdouts_1[272] << testdouts_1[273] << testdouts_1[274] << testdouts_1[275] << testdouts_1[276] << testdouts_1[277] << testdouts_1[278] << testdouts_1[279] << testdouts_1[280] << testdouts_1[281] << testdouts_1[282] << testdouts_1[283] << testdouts_1[284] << testdouts_1[285] << testdouts_1[286] << testdouts_1[287] << testdouts_1[288] << testdouts_1[289] << testdouts_1[290] << testdouts_1[291] << testdouts_1[292] << testdouts_1[293] << testdouts_1[294] << testdouts_1[295] << testdouts_1[296] << testdouts_1[297] << testdouts_1[298] << testdouts_1[299] << testdouts_1[300] << testdouts_1[301] << testdouts_1[302] << testdouts_1[303] << testdouts_1[304] << testdouts_1[305] << testdouts_1[306] << testdouts_1[307] << testdouts_1[308] << testdouts_1[309] << testdouts_1[310] << testdouts_1[311] << testdouts_1[312] << testdouts_1[313] << testdouts_1[314] << testcode_x << testcode;
    SC_METHOD(assign_testdout_3);
    sensitive << testdouts_2[0] << testdouts_2[1] << testdouts_2[2] << testdouts_2[3] << testdouts_2[4] << testdouts_2[5] << testdouts_2[6] << testdouts_2[7] << testdouts_2[8] << testdouts_2[9] << testdouts_2[10] << testdouts_2[11] << testdouts_2[12] << testdouts_2[13] << testdouts_2[14] << testdouts_2[15] << testdouts_2[16] << testdouts_2[17] << testdouts_2[18] << testdouts_2[19] << testdouts_2[20] << testdouts_2[21] << testdouts_2[22] << testdouts_2[23] << testdouts_2[24] << testdouts_2[25] << testdouts_2[26] << testdouts_2[27] << testdouts_2[28] << testdouts_2[29] << testdouts_2[30] << testdouts_2[31] << testdouts_2[32] << testdouts_2[33] << testdouts_2[34] << testdouts_2[35] << testdouts_2[36] << testdouts_2[37] << testdouts_2[38] << testdouts_2[39] << testdouts_2[40] << testdouts_2[41] << testdouts_2[42] << testdouts_2[43] << testdouts_2[44] << testdouts_2[45] << testdouts_2[46] << testdouts_2[47] << testdouts_2[48] << testdouts_2[49] << testdouts_2[50] << testdouts_2[51] << testdouts_2[52] << testdouts_2[53] << testdouts_2[54] << testdouts_2[55] << testdouts_2[56] << testdouts_2[57] << testdouts_2[58] << testdouts_2[59] << testdouts_2[60] << testdouts_2[61] << testdouts_2[62] << testdouts_2[63] << testdouts_2[64] << testdouts_2[65] << testdouts_2[66] << testdouts_2[67] << testdouts_2[68] << testdouts_2[69] << testdouts_2[70] << testdouts_2[71] << testdouts_2[72] << testdouts_2[73] << testdouts_2[74] << testdouts_2[75] << testdouts_2[76] << testdouts_2[77] << testdouts_2[78] << testdouts_2[79] << testdouts_2[80] << testdouts_2[81] << testdouts_2[82] << testdouts_2[83] << testdouts_2[84] << testdouts_2[85] << testdouts_2[86] << testdouts_2[87] << testdouts_2[88] << testdouts_2[89] << testdouts_2[90] << testdouts_2[91] << testdouts_2[92] << testdouts_2[93] << testdouts_2[94] << testdouts_2[95] << testdouts_2[96] << testdouts_2[97] << testdouts_2[98] << testdouts_2[99] << testdouts_2[100] << testdouts_2[101] << testdouts_2[102] << testdouts_2[103] << testdouts_2[104] << testdouts_2[105] << testdouts_2[106] << testdouts_2[107] << testdouts_2[108] << testdouts_2[109] << testdouts_2[110] << testdouts_2[111] << testdouts_2[112] << testdouts_2[113] << testdouts_2[114] << testdouts_2[115] << testdouts_2[116] << testdouts_2[117] << testdouts_2[118] << testdouts_2[119] << testdouts_2[120] << testdouts_2[121] << testdouts_2[122] << testdouts_2[123] << testdouts_2[124] << testdouts_2[125] << testdouts_2[126] << testdouts_2[127] << testdouts_2[128] << testdouts_2[129] << testdouts_2[130] << testdouts_2[131] << testdouts_2[132] << testdouts_2[133] << testdouts_2[134] << testdouts_2[135] << testdouts_2[136] << testdouts_2[137] << testdouts_2[138] << testdouts_2[139] << testdouts_2[140] << testdouts_2[141] << testdouts_2[142] << testdouts_2[143] << testdouts_2[144] << testdouts_2[145] << testdouts_2[146] << testdouts_2[147] << testdouts_2[148] << testdouts_2[149] << testdouts_2[150] << testdouts_2[151] << testdouts_2[152] << testdouts_2[153] << testdouts_2[154] << testdouts_2[155] << testdouts_2[156] << testdouts_2[157] << testdouts_2[158] << testdouts_2[159] << testdouts_2[160] << testdouts_2[161] << testdouts_2[162] << testdouts_2[163] << testdouts_2[164] << testdouts_2[165] << testdouts_2[166] << testdouts_2[167] << testdouts_2[168] << testdouts_2[169] << testdouts_2[170] << testdouts_2[171] << testdouts_2[172] << testdouts_2[173] << testdouts_2[174] << testdouts_2[175] << testdouts_2[176] << testdouts_2[177] << testdouts_2[178] << testdouts_2[179] << testdouts_2[180] << testdouts_2[181] << testdouts_2[182] << testdouts_2[183] << testdouts_2[184] << testdouts_2[185] << testdouts_2[186] << testdouts_2[187] << testdouts_2[188] << testdouts_2[189] << testdouts_2[190] << testdouts_2[191] << testdouts_2[192] << testdouts_2[193] << testdouts_2[194] << testdouts_2[195] << testdouts_2[196] << testdouts_2[197] << testdouts_2[198] << testdouts_2[199] << testdouts_2[200] << testdouts_2[201] << testdouts_2[202] << testdouts_2[203] << testdouts_2[204] << testdouts_2[205] << testdouts_2[206] << testdouts_2[207] << testdouts_2[208] << testdouts_2[209] << testdouts_2[210] << testdouts_2[211] << testdouts_2[212] << testdouts_2[213] << testdouts_2[214] << testdouts_2[215] << testdouts_2[216] << testdouts_2[217] << testdouts_2[218] << testdouts_2[219] << testdouts_2[220] << testdouts_2[221] << testdouts_2[222] << testdouts_2[223] << testdouts_2[224] << testdouts_2[225] << testdouts_2[226] << testdouts_2[227] << testdouts_2[228] << testdouts_2[229] << testdouts_2[230] << testdouts_2[231] << testdouts_2[232] << testdouts_2[233] << testdouts_2[234] << testdouts_2[235] << testdouts_2[236] << testdouts_2[237] << testdouts_2[238] << testdouts_2[239] << testdouts_2[240] << testdouts_2[241] << testdouts_2[242] << testdouts_2[243] << testdouts_2[244] << testdouts_2[245] << testdouts_2[246] << testdouts_2[247] << testdouts_2[248] << testdouts_2[249] << testdouts_2[250] << testdouts_2[251] << testdouts_2[252] << testdouts_2[253] << testdouts_2[254] << testdouts_2[255] << testdouts_2[256] << testdouts_2[257] << testdouts_2[258] << testdouts_2[259] << testdouts_2[260] << testdouts_2[261] << testdouts_2[262] << testdouts_2[263] << testdouts_2[264] << testdouts_2[265] << testdouts_2[266] << testdouts_2[267] << testdouts_2[268] << testdouts_2[269] << testdouts_2[270] << testdouts_2[271] << testdouts_2[272] << testdouts_2[273] << testdouts_2[274] << testdouts_2[275] << testdouts_2[276] << testdouts_2[277] << testdouts_2[278] << testdouts_2[279] << testdouts_2[280] << testdouts_2[281] << testdouts_2[282] << testdouts_2[283] << testdouts_2[284] << testdouts_2[285] << testdouts_2[286] << testdouts_2[287] << testdouts_2[288] << testdouts_2[289] << testdouts_2[290] << testdouts_2[291] << testdouts_2[292] << testdouts_2[293] << testdouts_2[294] << testdouts_2[295] << testdouts_2[296] << testdouts_2[297] << testdouts_2[298] << testdouts_2[299] << testdouts_2[300] << testdouts_2[301] << testdouts_2[302] << testdouts_2[303] << testdouts_2[304] << testdouts_2[305] << testdouts_2[306] << testdouts_2[307] << testdouts_2[308] << testdouts_2[309] << testdouts_2[310] << testdouts_2[311] << testdouts_2[312] << testdouts_2[313] << testdouts_2[314] << testcode_x << testcode;
    SC_METHOD(assign_testdout_4);
    sensitive << testdouts_3[0] << testdouts_3[1] << testdouts_3[2] << testdouts_3[3] << testdouts_3[4] << testdouts_3[5] << testdouts_3[6] << testdouts_3[7] << testdouts_3[8] << testdouts_3[9] << testdouts_3[10] << testdouts_3[11] << testdouts_3[12] << testdouts_3[13] << testdouts_3[14] << testdouts_3[15] << testdouts_3[16] << testdouts_3[17] << testdouts_3[18] << testdouts_3[19] << testdouts_3[20] << testdouts_3[21] << testdouts_3[22] << testdouts_3[23] << testdouts_3[24] << testdouts_3[25] << testdouts_3[26] << testdouts_3[27] << testdouts_3[28] << testdouts_3[29] << testdouts_3[30] << testdouts_3[31] << testdouts_3[32] << testdouts_3[33] << testdouts_3[34] << testdouts_3[35] << testdouts_3[36] << testdouts_3[37] << testdouts_3[38] << testdouts_3[39] << testdouts_3[40] << testdouts_3[41] << testdouts_3[42] << testdouts_3[43] << testdouts_3[44] << testdouts_3[45] << testdouts_3[46] << testdouts_3[47] << testdouts_3[48] << testdouts_3[49] << testdouts_3[50] << testdouts_3[51] << testdouts_3[52] << testdouts_3[53] << testdouts_3[54] << testdouts_3[55] << testdouts_3[56] << testdouts_3[57] << testdouts_3[58] << testdouts_3[59] << testdouts_3[60] << testdouts_3[61] << testdouts_3[62] << testdouts_3[63] << testdouts_3[64] << testdouts_3[65] << testdouts_3[66] << testdouts_3[67] << testdouts_3[68] << testdouts_3[69] << testdouts_3[70] << testdouts_3[71] << testdouts_3[72] << testdouts_3[73] << testdouts_3[74] << testdouts_3[75] << testdouts_3[76] << testdouts_3[77] << testdouts_3[78] << testdouts_3[79] << testdouts_3[80] << testdouts_3[81] << testdouts_3[82] << testdouts_3[83] << testdouts_3[84] << testdouts_3[85] << testdouts_3[86] << testdouts_3[87] << testdouts_3[88] << testdouts_3[89] << testdouts_3[90] << testdouts_3[91] << testdouts_3[92] << testdouts_3[93] << testdouts_3[94] << testdouts_3[95] << testdouts_3[96] << testdouts_3[97] << testdouts_3[98] << testdouts_3[99] << testdouts_3[100] << testdouts_3[101] << testdouts_3[102] << testdouts_3[103] << testdouts_3[104] << testdouts_3[105] << testdouts_3[106] << testdouts_3[107] << testdouts_3[108] << testdouts_3[109] << testdouts_3[110] << testdouts_3[111] << testdouts_3[112] << testdouts_3[113] << testdouts_3[114] << testdouts_3[115] << testdouts_3[116] << testdouts_3[117] << testdouts_3[118] << testdouts_3[119] << testdouts_3[120] << testdouts_3[121] << testdouts_3[122] << testdouts_3[123] << testdouts_3[124] << testdouts_3[125] << testdouts_3[126] << testdouts_3[127] << testdouts_3[128] << testdouts_3[129] << testdouts_3[130] << testdouts_3[131] << testdouts_3[132] << testdouts_3[133] << testdouts_3[134] << testdouts_3[135] << testdouts_3[136] << testdouts_3[137] << testdouts_3[138] << testdouts_3[139] << testdouts_3[140] << testdouts_3[141] << testdouts_3[142] << testdouts_3[143] << testdouts_3[144] << testdouts_3[145] << testdouts_3[146] << testdouts_3[147] << testdouts_3[148] << testdouts_3[149] << testdouts_3[150] << testdouts_3[151] << testdouts_3[152] << testdouts_3[153] << testdouts_3[154] << testdouts_3[155] << testdouts_3[156] << testdouts_3[157] << testdouts_3[158] << testdouts_3[159] << testdouts_3[160] << testdouts_3[161] << testdouts_3[162] << testdouts_3[163] << testdouts_3[164] << testdouts_3[165] << testdouts_3[166] << testdouts_3[167] << testdouts_3[168] << testdouts_3[169] << testdouts_3[170] << testdouts_3[171] << testdouts_3[172] << testdouts_3[173] << testdouts_3[174] << testdouts_3[175] << testdouts_3[176] << testdouts_3[177] << testdouts_3[178] << testdouts_3[179] << testdouts_3[180] << testdouts_3[181] << testdouts_3[182] << testdouts_3[183] << testdouts_3[184] << testdouts_3[185] << testdouts_3[186] << testdouts_3[187] << testdouts_3[188] << testdouts_3[189] << testdouts_3[190] << testdouts_3[191] << testdouts_3[192] << testdouts_3[193] << testdouts_3[194] << testdouts_3[195] << testdouts_3[196] << testdouts_3[197] << testdouts_3[198] << testdouts_3[199] << testdouts_3[200] << testdouts_3[201] << testdouts_3[202] << testdouts_3[203] << testdouts_3[204] << testdouts_3[205] << testdouts_3[206] << testdouts_3[207] << testdouts_3[208] << testdouts_3[209] << testdouts_3[210] << testdouts_3[211] << testdouts_3[212] << testdouts_3[213] << testdouts_3[214] << testdouts_3[215] << testdouts_3[216] << testdouts_3[217] << testdouts_3[218] << testdouts_3[219] << testdouts_3[220] << testdouts_3[221] << testdouts_3[222] << testdouts_3[223] << testdouts_3[224] << testdouts_3[225] << testdouts_3[226] << testdouts_3[227] << testdouts_3[228] << testdouts_3[229] << testdouts_3[230] << testdouts_3[231] << testdouts_3[232] << testdouts_3[233] << testdouts_3[234] << testdouts_3[235] << testdouts_3[236] << testdouts_3[237] << testdouts_3[238] << testdouts_3[239] << testdouts_3[240] << testdouts_3[241] << testdouts_3[242] << testdouts_3[243] << testdouts_3[244] << testdouts_3[245] << testdouts_3[246] << testdouts_3[247] << testdouts_3[248] << testdouts_3[249] << testdouts_3[250] << testdouts_3[251] << testdouts_3[252] << testdouts_3[253] << testdouts_3[254] << testdouts_3[255] << testdouts_3[256] << testdouts_3[257] << testdouts_3[258] << testdouts_3[259] << testdouts_3[260] << testdouts_3[261] << testdouts_3[262] << testdouts_3[263] << testdouts_3[264] << testdouts_3[265] << testdouts_3[266] << testdouts_3[267] << testdouts_3[268] << testdouts_3[269] << testdouts_3[270] << testdouts_3[271] << testdouts_3[272] << testdouts_3[273] << testdouts_3[274] << testdouts_3[275] << testdouts_3[276] << testdouts_3[277] << testdouts_3[278] << testdouts_3[279] << testdouts_3[280] << testdouts_3[281] << testdouts_3[282] << testdouts_3[283] << testdouts_3[284] << testdouts_3[285] << testdouts_3[286] << testdouts_3[287] << testdouts_3[288] << testdouts_3[289] << testdouts_3[290] << testdouts_3[291] << testdouts_3[292] << testdouts_3[293] << testdouts_3[294] << testdouts_3[295] << testdouts_3[296] << testdouts_3[297] << testdouts_3[298] << testdouts_3[299] << testdouts_3[300] << testdouts_3[301] << testdouts_3[302] << testdouts_3[303] << testdouts_3[304] << testdouts_3[305] << testdouts_3[306] << testdouts_3[307] << testdouts_3[308] << testdouts_3[309] << testdouts_3[310] << testdouts_3[311] << testdouts_3[312] << testdouts_3[313] << testdouts_3[314] << testcode_x << testcode;
    SC_METHOD(assign_testdout_5);
    sensitive << testdouts_4[0] << testdouts_4[1] << testdouts_4[2] << testdouts_4[3] << testdouts_4[4] << testdouts_4[5] << testdouts_4[6] << testdouts_4[7] << testdouts_4[8] << testdouts_4[9] << testdouts_4[10] << testdouts_4[11] << testdouts_4[12] << testdouts_4[13] << testdouts_4[14] << testdouts_4[15] << testdouts_4[16] << testdouts_4[17] << testdouts_4[18] << testdouts_4[19] << testdouts_4[20] << testdouts_4[21] << testdouts_4[22] << testdouts_4[23] << testdouts_4[24] << testdouts_4[25] << testdouts_4[26] << testdouts_4[27] << testdouts_4[28] << testdouts_4[29] << testdouts_4[30] << testdouts_4[31] << testdouts_4[32] << testdouts_4[33] << testdouts_4[34] << testdouts_4[35] << testdouts_4[36] << testdouts_4[37] << testdouts_4[38] << testdouts_4[39] << testdouts_4[40] << testdouts_4[41] << testdouts_4[42] << testdouts_4[43] << testdouts_4[44] << testdouts_4[45] << testdouts_4[46] << testdouts_4[47] << testdouts_4[48] << testdouts_4[49] << testdouts_4[50] << testdouts_4[51] << testdouts_4[52] << testdouts_4[53] << testdouts_4[54] << testdouts_4[55] << testdouts_4[56] << testdouts_4[57] << testdouts_4[58] << testdouts_4[59] << testdouts_4[60] << testdouts_4[61] << testdouts_4[62] << testdouts_4[63] << testdouts_4[64] << testdouts_4[65] << testdouts_4[66] << testdouts_4[67] << testdouts_4[68] << testdouts_4[69] << testdouts_4[70] << testdouts_4[71] << testdouts_4[72] << testdouts_4[73] << testdouts_4[74] << testdouts_4[75] << testdouts_4[76] << testdouts_4[77] << testdouts_4[78] << testdouts_4[79] << testdouts_4[80] << testdouts_4[81] << testdouts_4[82] << testdouts_4[83] << testdouts_4[84] << testdouts_4[85] << testdouts_4[86] << testdouts_4[87] << testdouts_4[88] << testdouts_4[89] << testdouts_4[90] << testdouts_4[91] << testdouts_4[92] << testdouts_4[93] << testdouts_4[94] << testdouts_4[95] << testdouts_4[96] << testdouts_4[97] << testdouts_4[98] << testdouts_4[99] << testdouts_4[100] << testdouts_4[101] << testdouts_4[102] << testdouts_4[103] << testdouts_4[104] << testdouts_4[105] << testdouts_4[106] << testdouts_4[107] << testdouts_4[108] << testdouts_4[109] << testdouts_4[110] << testdouts_4[111] << testdouts_4[112] << testdouts_4[113] << testdouts_4[114] << testdouts_4[115] << testdouts_4[116] << testdouts_4[117] << testdouts_4[118] << testdouts_4[119] << testdouts_4[120] << testdouts_4[121] << testdouts_4[122] << testdouts_4[123] << testdouts_4[124] << testdouts_4[125] << testdouts_4[126] << testdouts_4[127] << testdouts_4[128] << testdouts_4[129] << testdouts_4[130] << testdouts_4[131] << testdouts_4[132] << testdouts_4[133] << testdouts_4[134] << testdouts_4[135] << testdouts_4[136] << testdouts_4[137] << testdouts_4[138] << testdouts_4[139] << testdouts_4[140] << testdouts_4[141] << testdouts_4[142] << testdouts_4[143] << testdouts_4[144] << testdouts_4[145] << testdouts_4[146] << testdouts_4[147] << testdouts_4[148] << testdouts_4[149] << testdouts_4[150] << testdouts_4[151] << testdouts_4[152] << testdouts_4[153] << testdouts_4[154] << testdouts_4[155] << testdouts_4[156] << testdouts_4[157] << testdouts_4[158] << testdouts_4[159] << testdouts_4[160] << testdouts_4[161] << testdouts_4[162] << testdouts_4[163] << testdouts_4[164] << testdouts_4[165] << testdouts_4[166] << testdouts_4[167] << testdouts_4[168] << testdouts_4[169] << testdouts_4[170] << testdouts_4[171] << testdouts_4[172] << testdouts_4[173] << testdouts_4[174] << testdouts_4[175] << testdouts_4[176] << testdouts_4[177] << testdouts_4[178] << testdouts_4[179] << testdouts_4[180] << testdouts_4[181] << testdouts_4[182] << testdouts_4[183] << testdouts_4[184] << testdouts_4[185] << testdouts_4[186] << testdouts_4[187] << testdouts_4[188] << testdouts_4[189] << testdouts_4[190] << testdouts_4[191] << testdouts_4[192] << testdouts_4[193] << testdouts_4[194] << testdouts_4[195] << testdouts_4[196] << testdouts_4[197] << testdouts_4[198] << testdouts_4[199] << testdouts_4[200] << testdouts_4[201] << testdouts_4[202] << testdouts_4[203] << testdouts_4[204] << testdouts_4[205] << testdouts_4[206] << testdouts_4[207] << testdouts_4[208] << testdouts_4[209] << testdouts_4[210] << testdouts_4[211] << testdouts_4[212] << testdouts_4[213] << testdouts_4[214] << testdouts_4[215] << testdouts_4[216] << testdouts_4[217] << testdouts_4[218] << testdouts_4[219] << testdouts_4[220] << testdouts_4[221] << testdouts_4[222] << testdouts_4[223] << testdouts_4[224] << testdouts_4[225] << testdouts_4[226] << testdouts_4[227] << testdouts_4[228] << testdouts_4[229] << testdouts_4[230] << testdouts_4[231] << testdouts_4[232] << testdouts_4[233] << testdouts_4[234] << testdouts_4[235] << testdouts_4[236] << testdouts_4[237] << testdouts_4[238] << testdouts_4[239] << testdouts_4[240] << testdouts_4[241] << testdouts_4[242] << testdouts_4[243] << testdouts_4[244] << testdouts_4[245] << testdouts_4[246] << testdouts_4[247] << testdouts_4[248] << testdouts_4[249] << testdouts_4[250] << testdouts_4[251] << testdouts_4[252] << testdouts_4[253] << testdouts_4[254] << testdouts_4[255] << testdouts_4[256] << testdouts_4[257] << testdouts_4[258] << testdouts_4[259] << testdouts_4[260] << testdouts_4[261] << testdouts_4[262] << testdouts_4[263] << testdouts_4[264] << testdouts_4[265] << testdouts_4[266] << testdouts_4[267] << testdouts_4[268] << testdouts_4[269] << testdouts_4[270] << testdouts_4[271] << testdouts_4[272] << testdouts_4[273] << testdouts_4[274] << testdouts_4[275] << testdouts_4[276] << testdouts_4[277] << testdouts_4[278] << testdouts_4[279] << testdouts_4[280] << testdouts_4[281] << testdouts_4[282] << testdouts_4[283] << testdouts_4[284] << testdouts_4[285] << testdouts_4[286] << testdouts_4[287] << testdouts_4[288] << testdouts_4[289] << testdouts_4[290] << testdouts_4[291] << testdouts_4[292] << testdouts_4[293] << testdouts_4[294] << testdouts_4[295] << testdouts_4[296] << testdouts_4[297] << testdouts_4[298] << testdouts_4[299] << testdouts_4[300] << testdouts_4[301] << testdouts_4[302] << testdouts_4[303] << testdouts_4[304] << testdouts_4[305] << testdouts_4[306] << testdouts_4[307] << testdouts_4[308] << testdouts_4[309] << testdouts_4[310] << testdouts_4[311] << testdouts_4[312] << testdouts_4[313] << testdouts_4[314];
    SC_METHOD(assign_testdout_6);
    sensitive << testdouts_5[0] << testdouts_5[1] << testdouts_5[2] << testdouts_5[3] << testdouts_5[4] << testdouts_5[5] << testdouts_5[6] << testdouts_5[7] << testdouts_5[8] << testdouts_5[9] << testdouts_5[10] << testdouts_5[11] << testdouts_5[12] << testdouts_5[13] << testdouts_5[14] << testdouts_5[15] << testdouts_5[16] << testdouts_5[17] << testdouts_5[18] << testdouts_5[19] << testdouts_5[20] << testdouts_5[21] << testdouts_5[22] << testdouts_5[23] << testdouts_5[24] << testdouts_5[25] << testdouts_5[26] << testdouts_5[27] << testdouts_5[28] << testdouts_5[29] << testdouts_5[30] << testdouts_5[31] << testdouts_5[32] << testdouts_5[33] << testdouts_5[34] << testdouts_5[35] << testdouts_5[36] << testdouts_5[37] << testdouts_5[38] << testdouts_5[39] << testdouts_5[40] << testdouts_5[41] << testdouts_5[42] << testdouts_5[43] << testdouts_5[44] << testdouts_5[45] << testdouts_5[46] << testdouts_5[47] << testdouts_5[48] << testdouts_5[49] << testdouts_5[50] << testdouts_5[51] << testdouts_5[52] << testdouts_5[53] << testdouts_5[54] << testdouts_5[55] << testdouts_5[56] << testdouts_5[57] << testdouts_5[58] << testdouts_5[59] << testdouts_5[60] << testdouts_5[61] << testdouts_5[62] << testdouts_5[63] << testdouts_5[64] << testdouts_5[65] << testdouts_5[66] << testdouts_5[67] << testdouts_5[68] << testdouts_5[69] << testdouts_5[70] << testdouts_5[71] << testdouts_5[72] << testdouts_5[73] << testdouts_5[74] << testdouts_5[75] << testdouts_5[76] << testdouts_5[77] << testdouts_5[78] << testdouts_5[79] << testdouts_5[80] << testdouts_5[81] << testdouts_5[82] << testdouts_5[83] << testdouts_5[84] << testdouts_5[85] << testdouts_5[86] << testdouts_5[87] << testdouts_5[88] << testdouts_5[89] << testdouts_5[90] << testdouts_5[91] << testdouts_5[92] << testdouts_5[93] << testdouts_5[94] << testdouts_5[95] << testdouts_5[96] << testdouts_5[97] << testdouts_5[98] << testdouts_5[99] << testdouts_5[100] << testdouts_5[101] << testdouts_5[102] << testdouts_5[103] << testdouts_5[104] << testdouts_5[105] << testdouts_5[106] << testdouts_5[107] << testdouts_5[108] << testdouts_5[109] << testdouts_5[110] << testdouts_5[111] << testdouts_5[112] << testdouts_5[113] << testdouts_5[114] << testdouts_5[115] << testdouts_5[116] << testdouts_5[117] << testdouts_5[118] << testdouts_5[119] << testdouts_5[120] << testdouts_5[121] << testdouts_5[122] << testdouts_5[123] << testdouts_5[124] << testdouts_5[125] << testdouts_5[126] << testdouts_5[127] << testdouts_5[128] << testdouts_5[129] << testdouts_5[130] << testdouts_5[131] << testdouts_5[132] << testdouts_5[133] << testdouts_5[134] << testdouts_5[135] << testdouts_5[136] << testdouts_5[137] << testdouts_5[138] << testdouts_5[139] << testdouts_5[140] << testdouts_5[141] << testdouts_5[142] << testdouts_5[143] << testdouts_5[144] << testdouts_5[145] << testdouts_5[146] << testdouts_5[147] << testdouts_5[148] << testdouts_5[149] << testdouts_5[150] << testdouts_5[151] << testdouts_5[152] << testdouts_5[153] << testdouts_5[154] << testdouts_5[155] << testdouts_5[156] << testdouts_5[157] << testdouts_5[158] << testdouts_5[159] << testdouts_5[160] << testdouts_5[161] << testdouts_5[162] << testdouts_5[163] << testdouts_5[164] << testdouts_5[165] << testdouts_5[166] << testdouts_5[167] << testdouts_5[168] << testdouts_5[169] << testdouts_5[170] << testdouts_5[171] << testdouts_5[172] << testdouts_5[173] << testdouts_5[174] << testdouts_5[175] << testdouts_5[176] << testdouts_5[177] << testdouts_5[178] << testdouts_5[179] << testdouts_5[180] << testdouts_5[181] << testdouts_5[182] << testdouts_5[183] << testdouts_5[184] << testdouts_5[185] << testdouts_5[186] << testdouts_5[187] << testdouts_5[188] << testdouts_5[189] << testdouts_5[190] << testdouts_5[191] << testdouts_5[192] << testdouts_5[193] << testdouts_5[194] << testdouts_5[195] << testdouts_5[196] << testdouts_5[197] << testdouts_5[198] << testdouts_5[199] << testdouts_5[200] << testdouts_5[201] << testdouts_5[202] << testdouts_5[203] << testdouts_5[204] << testdouts_5[205] << testdouts_5[206] << testdouts_5[207] << testdouts_5[208] << testdouts_5[209] << testdouts_5[210] << testdouts_5[211] << testdouts_5[212] << testdouts_5[213] << testdouts_5[214] << testdouts_5[215] << testdouts_5[216] << testdouts_5[217] << testdouts_5[218] << testdouts_5[219] << testdouts_5[220] << testdouts_5[221] << testdouts_5[222] << testdouts_5[223] << testdouts_5[224] << testdouts_5[225] << testdouts_5[226] << testdouts_5[227] << testdouts_5[228] << testdouts_5[229] << testdouts_5[230] << testdouts_5[231] << testdouts_5[232] << testdouts_5[233] << testdouts_5[234] << testdouts_5[235] << testdouts_5[236] << testdouts_5[237] << testdouts_5[238] << testdouts_5[239] << testdouts_5[240] << testdouts_5[241] << testdouts_5[242] << testdouts_5[243] << testdouts_5[244] << testdouts_5[245] << testdouts_5[246] << testdouts_5[247] << testdouts_5[248] << testdouts_5[249] << testdouts_5[250] << testdouts_5[251] << testdouts_5[252] << testdouts_5[253] << testdouts_5[254] << testdouts_5[255] << testdouts_5[256] << testdouts_5[257] << testdouts_5[258] << testdouts_5[259] << testdouts_5[260] << testdouts_5[261] << testdouts_5[262] << testdouts_5[263] << testdouts_5[264] << testdouts_5[265] << testdouts_5[266] << testdouts_5[267] << testdouts_5[268] << testdouts_5[269] << testdouts_5[270] << testdouts_5[271] << testdouts_5[272] << testdouts_5[273] << testdouts_5[274] << testdouts_5[275] << testdouts_5[276] << testdouts_5[277] << testdouts_5[278] << testdouts_5[279] << testdouts_5[280] << testdouts_5[281] << testdouts_5[282] << testdouts_5[283] << testdouts_5[284] << testdouts_5[285] << testdouts_5[286] << testdouts_5[287] << testdouts_5[288] << testdouts_5[289] << testdouts_5[290] << testdouts_5[291] << testdouts_5[292] << testdouts_5[293] << testdouts_5[294] << testdouts_5[295] << testdouts_5[296] << testdouts_5[297] << testdouts_5[298] << testdouts_5[299] << testdouts_5[300] << testdouts_5[301] << testdouts_5[302] << testdouts_5[303] << testdouts_5[304] << testdouts_5[305] << testdouts_5[306] << testdouts_5[307] << testdouts_5[308] << testdouts_5[309] << testdouts_5[310] << testdouts_5[311] << testdouts_5[312] << testdouts_5[313] << testdouts_5[314];
    SC_METHOD(assign_testdout_7);
    sensitive << testdouts_6[0] << testdouts_6[1] << testdouts_6[2] << testdouts_6[3] << testdouts_6[4] << testdouts_6[5] << testdouts_6[6] << testdouts_6[7] << testdouts_6[8] << testdouts_6[9] << testdouts_6[10] << testdouts_6[11] << testdouts_6[12] << testdouts_6[13] << testdouts_6[14] << testdouts_6[15] << testdouts_6[16] << testdouts_6[17] << testdouts_6[18] << testdouts_6[19] << testdouts_6[20] << testdouts_6[21] << testdouts_6[22] << testdouts_6[23] << testdouts_6[24] << testdouts_6[25] << testdouts_6[26] << testdouts_6[27] << testdouts_6[28] << testdouts_6[29] << testdouts_6[30] << testdouts_6[31] << testdouts_6[32] << testdouts_6[33] << testdouts_6[34] << testdouts_6[35] << testdouts_6[36] << testdouts_6[37] << testdouts_6[38] << testdouts_6[39] << testdouts_6[40] << testdouts_6[41] << testdouts_6[42] << testdouts_6[43] << testdouts_6[44] << testdouts_6[45] << testdouts_6[46] << testdouts_6[47] << testdouts_6[48] << testdouts_6[49] << testdouts_6[50] << testdouts_6[51] << testdouts_6[52] << testdouts_6[53] << testdouts_6[54] << testdouts_6[55] << testdouts_6[56] << testdouts_6[57] << testdouts_6[58] << testdouts_6[59] << testdouts_6[60] << testdouts_6[61] << testdouts_6[62] << testdouts_6[63] << testdouts_6[64] << testdouts_6[65] << testdouts_6[66] << testdouts_6[67] << testdouts_6[68] << testdouts_6[69] << testdouts_6[70] << testdouts_6[71] << testdouts_6[72] << testdouts_6[73] << testdouts_6[74] << testdouts_6[75] << testdouts_6[76] << testdouts_6[77] << testdouts_6[78] << testdouts_6[79] << testdouts_6[80] << testdouts_6[81] << testdouts_6[82] << testdouts_6[83] << testdouts_6[84] << testdouts_6[85] << testdouts_6[86] << testdouts_6[87] << testdouts_6[88] << testdouts_6[89] << testdouts_6[90] << testdouts_6[91] << testdouts_6[92] << testdouts_6[93] << testdouts_6[94] << testdouts_6[95] << testdouts_6[96] << testdouts_6[97] << testdouts_6[98] << testdouts_6[99] << testdouts_6[100] << testdouts_6[101] << testdouts_6[102] << testdouts_6[103] << testdouts_6[104] << testdouts_6[105] << testdouts_6[106] << testdouts_6[107] << testdouts_6[108] << testdouts_6[109] << testdouts_6[110] << testdouts_6[111] << testdouts_6[112] << testdouts_6[113] << testdouts_6[114] << testdouts_6[115] << testdouts_6[116] << testdouts_6[117] << testdouts_6[118] << testdouts_6[119] << testdouts_6[120] << testdouts_6[121] << testdouts_6[122] << testdouts_6[123] << testdouts_6[124] << testdouts_6[125] << testdouts_6[126] << testdouts_6[127] << testdouts_6[128] << testdouts_6[129] << testdouts_6[130] << testdouts_6[131] << testdouts_6[132] << testdouts_6[133] << testdouts_6[134] << testdouts_6[135] << testdouts_6[136] << testdouts_6[137] << testdouts_6[138] << testdouts_6[139] << testdouts_6[140] << testdouts_6[141] << testdouts_6[142] << testdouts_6[143] << testdouts_6[144] << testdouts_6[145] << testdouts_6[146] << testdouts_6[147] << testdouts_6[148] << testdouts_6[149] << testdouts_6[150] << testdouts_6[151] << testdouts_6[152] << testdouts_6[153] << testdouts_6[154] << testdouts_6[155] << testdouts_6[156] << testdouts_6[157] << testdouts_6[158] << testdouts_6[159] << testdouts_6[160] << testdouts_6[161] << testdouts_6[162] << testdouts_6[163] << testdouts_6[164] << testdouts_6[165] << testdouts_6[166] << testdouts_6[167] << testdouts_6[168] << testdouts_6[169] << testdouts_6[170] << testdouts_6[171] << testdouts_6[172] << testdouts_6[173] << testdouts_6[174] << testdouts_6[175] << testdouts_6[176] << testdouts_6[177] << testdouts_6[178] << testdouts_6[179] << testdouts_6[180] << testdouts_6[181] << testdouts_6[182] << testdouts_6[183] << testdouts_6[184] << testdouts_6[185] << testdouts_6[186] << testdouts_6[187] << testdouts_6[188] << testdouts_6[189] << testdouts_6[190] << testdouts_6[191] << testdouts_6[192] << testdouts_6[193] << testdouts_6[194] << testdouts_6[195] << testdouts_6[196] << testdouts_6[197] << testdouts_6[198] << testdouts_6[199] << testdouts_6[200] << testdouts_6[201] << testdouts_6[202] << testdouts_6[203] << testdouts_6[204] << testdouts_6[205] << testdouts_6[206] << testdouts_6[207] << testdouts_6[208] << testdouts_6[209] << testdouts_6[210] << testdouts_6[211] << testdouts_6[212] << testdouts_6[213] << testdouts_6[214] << testdouts_6[215] << testdouts_6[216] << testdouts_6[217] << testdouts_6[218] << testdouts_6[219] << testdouts_6[220] << testdouts_6[221] << testdouts_6[222] << testdouts_6[223] << testdouts_6[224] << testdouts_6[225] << testdouts_6[226] << testdouts_6[227] << testdouts_6[228] << testdouts_6[229] << testdouts_6[230] << testdouts_6[231] << testdouts_6[232] << testdouts_6[233] << testdouts_6[234] << testdouts_6[235] << testdouts_6[236] << testdouts_6[237] << testdouts_6[238] << testdouts_6[239] << testdouts_6[240] << testdouts_6[241] << testdouts_6[242] << testdouts_6[243] << testdouts_6[244] << testdouts_6[245] << testdouts_6[246] << testdouts_6[247] << testdouts_6[248] << testdouts_6[249] << testdouts_6[250] << testdouts_6[251] << testdouts_6[252] << testdouts_6[253] << testdouts_6[254] << testdouts_6[255] << testdouts_6[256] << testdouts_6[257] << testdouts_6[258] << testdouts_6[259] << testdouts_6[260] << testdouts_6[261] << testdouts_6[262] << testdouts_6[263] << testdouts_6[264] << testdouts_6[265] << testdouts_6[266] << testdouts_6[267] << testdouts_6[268] << testdouts_6[269] << testdouts_6[270] << testdouts_6[271] << testdouts_6[272] << testdouts_6[273] << testdouts_6[274] << testdouts_6[275] << testdouts_6[276] << testdouts_6[277] << testdouts_6[278] << testdouts_6[279] << testdouts_6[280] << testdouts_6[281] << testdouts_6[282] << testdouts_6[283] << testdouts_6[284] << testdouts_6[285] << testdouts_6[286] << testdouts_6[287] << testdouts_6[288] << testdouts_6[289] << testdouts_6[290] << testdouts_6[291] << testdouts_6[292] << testdouts_6[293] << testdouts_6[294] << testdouts_6[295] << testdouts_6[296] << testdouts_6[297] << testdouts_6[298] << testdouts_6[299] << testdouts_6[300] << testdouts_6[301] << testdouts_6[302] << testdouts_6[303] << testdouts_6[304] << testdouts_6[305] << testdouts_6[306] << testdouts_6[307] << testdouts_6[308] << testdouts_6[309] << testdouts_6[310] << testdouts_6[311] << testdouts_6[312] << testdouts_6[313] << testdouts_6[314];
    SC_METHOD(assign_testdout_8);
    sensitive << testdouts_7[0] << testdouts_7[1] << testdouts_7[2] << testdouts_7[3] << testdouts_7[4] << testdouts_7[5] << testdouts_7[6] << testdouts_7[7] << testdouts_7[8] << testdouts_7[9] << testdouts_7[10] << testdouts_7[11] << testdouts_7[12] << testdouts_7[13] << testdouts_7[14] << testdouts_7[15] << testdouts_7[16] << testdouts_7[17] << testdouts_7[18] << testdouts_7[19] << testdouts_7[20] << testdouts_7[21] << testdouts_7[22] << testdouts_7[23] << testdouts_7[24] << testdouts_7[25] << testdouts_7[26] << testdouts_7[27] << testdouts_7[28] << testdouts_7[29] << testdouts_7[30] << testdouts_7[31] << testdouts_7[32] << testdouts_7[33] << testdouts_7[34] << testdouts_7[35] << testdouts_7[36] << testdouts_7[37] << testdouts_7[38] << testdouts_7[39] << testdouts_7[40] << testdouts_7[41] << testdouts_7[42] << testdouts_7[43] << testdouts_7[44] << testdouts_7[45] << testdouts_7[46] << testdouts_7[47] << testdouts_7[48] << testdouts_7[49] << testdouts_7[50] << testdouts_7[51] << testdouts_7[52] << testdouts_7[53] << testdouts_7[54] << testdouts_7[55] << testdouts_7[56] << testdouts_7[57] << testdouts_7[58] << testdouts_7[59] << testdouts_7[60] << testdouts_7[61] << testdouts_7[62] << testdouts_7[63] << testdouts_7[64] << testdouts_7[65] << testdouts_7[66] << testdouts_7[67] << testdouts_7[68] << testdouts_7[69] << testdouts_7[70] << testdouts_7[71] << testdouts_7[72] << testdouts_7[73] << testdouts_7[74] << testdouts_7[75] << testdouts_7[76] << testdouts_7[77] << testdouts_7[78] << testdouts_7[79] << testdouts_7[80] << testdouts_7[81] << testdouts_7[82] << testdouts_7[83] << testdouts_7[84] << testdouts_7[85] << testdouts_7[86] << testdouts_7[87] << testdouts_7[88] << testdouts_7[89] << testdouts_7[90] << testdouts_7[91] << testdouts_7[92] << testdouts_7[93] << testdouts_7[94] << testdouts_7[95] << testdouts_7[96] << testdouts_7[97] << testdouts_7[98] << testdouts_7[99] << testdouts_7[100] << testdouts_7[101] << testdouts_7[102] << testdouts_7[103] << testdouts_7[104] << testdouts_7[105] << testdouts_7[106] << testdouts_7[107] << testdouts_7[108] << testdouts_7[109] << testdouts_7[110] << testdouts_7[111] << testdouts_7[112] << testdouts_7[113] << testdouts_7[114] << testdouts_7[115] << testdouts_7[116] << testdouts_7[117] << testdouts_7[118] << testdouts_7[119] << testdouts_7[120] << testdouts_7[121] << testdouts_7[122] << testdouts_7[123] << testdouts_7[124] << testdouts_7[125] << testdouts_7[126] << testdouts_7[127] << testdouts_7[128] << testdouts_7[129] << testdouts_7[130] << testdouts_7[131] << testdouts_7[132] << testdouts_7[133] << testdouts_7[134] << testdouts_7[135] << testdouts_7[136] << testdouts_7[137] << testdouts_7[138] << testdouts_7[139] << testdouts_7[140] << testdouts_7[141] << testdouts_7[142] << testdouts_7[143] << testdouts_7[144] << testdouts_7[145] << testdouts_7[146] << testdouts_7[147] << testdouts_7[148] << testdouts_7[149] << testdouts_7[150] << testdouts_7[151] << testdouts_7[152] << testdouts_7[153] << testdouts_7[154] << testdouts_7[155] << testdouts_7[156] << testdouts_7[157] << testdouts_7[158] << testdouts_7[159] << testdouts_7[160] << testdouts_7[161] << testdouts_7[162] << testdouts_7[163] << testdouts_7[164] << testdouts_7[165] << testdouts_7[166] << testdouts_7[167] << testdouts_7[168] << testdouts_7[169] << testdouts_7[170] << testdouts_7[171] << testdouts_7[172] << testdouts_7[173] << testdouts_7[174] << testdouts_7[175] << testdouts_7[176] << testdouts_7[177] << testdouts_7[178] << testdouts_7[179] << testdouts_7[180] << testdouts_7[181] << testdouts_7[182] << testdouts_7[183] << testdouts_7[184] << testdouts_7[185] << testdouts_7[186] << testdouts_7[187] << testdouts_7[188] << testdouts_7[189] << testdouts_7[190] << testdouts_7[191] << testdouts_7[192] << testdouts_7[193] << testdouts_7[194] << testdouts_7[195] << testdouts_7[196] << testdouts_7[197] << testdouts_7[198] << testdouts_7[199] << testdouts_7[200] << testdouts_7[201] << testdouts_7[202] << testdouts_7[203] << testdouts_7[204] << testdouts_7[205] << testdouts_7[206] << testdouts_7[207] << testdouts_7[208] << testdouts_7[209] << testdouts_7[210] << testdouts_7[211] << testdouts_7[212] << testdouts_7[213] << testdouts_7[214] << testdouts_7[215] << testdouts_7[216] << testdouts_7[217] << testdouts_7[218] << testdouts_7[219] << testdouts_7[220] << testdouts_7[221] << testdouts_7[222] << testdouts_7[223] << testdouts_7[224] << testdouts_7[225] << testdouts_7[226] << testdouts_7[227] << testdouts_7[228] << testdouts_7[229] << testdouts_7[230] << testdouts_7[231] << testdouts_7[232] << testdouts_7[233] << testdouts_7[234] << testdouts_7[235] << testdouts_7[236] << testdouts_7[237] << testdouts_7[238] << testdouts_7[239] << testdouts_7[240] << testdouts_7[241] << testdouts_7[242] << testdouts_7[243] << testdouts_7[244] << testdouts_7[245] << testdouts_7[246] << testdouts_7[247] << testdouts_7[248] << testdouts_7[249] << testdouts_7[250] << testdouts_7[251] << testdouts_7[252] << testdouts_7[253] << testdouts_7[254] << testdouts_7[255] << testdouts_7[256] << testdouts_7[257] << testdouts_7[258] << testdouts_7[259] << testdouts_7[260] << testdouts_7[261] << testdouts_7[262] << testdouts_7[263] << testdouts_7[264] << testdouts_7[265] << testdouts_7[266] << testdouts_7[267] << testdouts_7[268] << testdouts_7[269] << testdouts_7[270] << testdouts_7[271] << testdouts_7[272] << testdouts_7[273] << testdouts_7[274] << testdouts_7[275] << testdouts_7[276] << testdouts_7[277] << testdouts_7[278] << testdouts_7[279] << testdouts_7[280] << testdouts_7[281] << testdouts_7[282] << testdouts_7[283] << testdouts_7[284] << testdouts_7[285] << testdouts_7[286] << testdouts_7[287] << testdouts_7[288] << testdouts_7[289] << testdouts_7[290] << testdouts_7[291] << testdouts_7[292] << testdouts_7[293] << testdouts_7[294] << testdouts_7[295] << testdouts_7[296] << testdouts_7[297] << testdouts_7[298] << testdouts_7[299] << testdouts_7[300] << testdouts_7[301] << testdouts_7[302] << testdouts_7[303] << testdouts_7[304] << testdouts_7[305] << testdouts_7[306] << testdouts_7[307] << testdouts_7[308] << testdouts_7[309] << testdouts_7[310] << testdouts_7[311] << testdouts_7[312] << testdouts_7[313] << testdouts_7[314];
    SC_METHOD(assign_lprxponcd_bypass_lane0);
    sensitive << lprxponcd_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_target_state_bist);
    sensitive << target_state_bist_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_bypass_lane2);
    sensitive << hsrxenterm_bypass_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_bypass_clklane);
    sensitive << lprxponulp_bypass_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_clk_en_lanes_bypass);
    sensitive << clk_en_lanes_bypass_rw << shutdownz_tester;
    SC_METHOD(assign_rx_des_en_lane3);
    sensitive << rx_des_en_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_lptxenpulldwn_bypass_lane0);
    sensitive << lptxenpulldwn_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_cb_ical_x4_ovr_en);
    sensitive << cb_ical_x4_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx_cb_pre_iref_on);
    sensitive << rx_cb_pre_iref_on_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_serial_clk_en_ovr_en);
    sensitive << rxclk_lbck_serial_clk_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_txstarttime);
    sensitive << txstarttime_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_pm_start_control_ovr_en);
    sensitive << rx0_pm_start_control_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_cal_done_ovr);
    sensitive << rx3_cal_done_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_bypass_lane0);
    sensitive << hsrxenterm_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxthssettle_ovr_en_datalanes);
    sensitive << hsrxthssettle_ovr_en_datalanes_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_phase_data_ovr);
    sensitive << rx0_rxhs_ddl_phase_data_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_setr_min);
    sensitive << setr_min_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_tune_ovr_en);
    sensitive << rx1_rxhs_ddl_tune_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_cb_on_ovr_en);
    sensitive << cb_on_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_m_ovr_en);
    sensitive << pll_m_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_ttawait);
    sensitive << ttawait_rw << shutdownz_tester;
    SC_METHOD(assign_recal_lane2_ovr);
    sensitive << recal_lane2_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_aligner_dk_cnf);
    sensitive << aligner_dk_cnf_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lb_prog_if);
    sensitive << rxclk_lb_prog_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_ddl_start_calibration_ovr);
    sensitive << rx2_ddl_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_tune_en_if);
    sensitive << rx2_rxhs_ddl_tune_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_tclk_miss_div2_ovr);
    sensitive << tclk_miss_div2_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_ddl_finished_ovr);
    sensitive << rx1_ddl_finished_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_txlp_eniocont_if);
    sensitive << rx0_txlp_eniocont_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_data_eq_if);
    sensitive << rx3_rxhs_data_eq_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_clk_from_long_channel_if);
    sensitive << rx1_rxhs_clk_from_long_channel_if_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_osc_pon_ovr_en);
    sensitive << rxclk_rxhs_osc_pon_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_feed_int_clk_ovr_en);
    sensitive << rx0_rxhs_feed_int_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_pm_man_error_latch);
    sensitive << rx3_pm_man_error_latch_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_bypass_lane1);
    sensitive << hsrx_en_bypass_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_rx_cb_lb_setrx_if);
    sensitive << rx_cb_lb_setrx_if_rw << shutdownz_tester;
    SC_METHOD(assign_polarity_lane2);
    sensitive << polarity_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_cb_atb_on_ovr);
    sensitive << cb_atb_on_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lb_sel_ovr_en);
    sensitive << rxclk_lb_sel_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_bypass_skew_machine);
    sensitive << bypass_skew_machine_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_data_retiming_out_swap_sel);
    sensitive << rx1_data_retiming_out_swap_sel_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_phase_change_p_ovr_en);
    sensitive << rx2_rxhs_ddl_phase_change_p_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pg_data_swap);
    sensitive << pg_data_swap_rw << shutdownz_tester;
    SC_METHOD(assign_recal_lane1_ovr);
    sensitive << recal_lane1_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_lane2);
    sensitive << bintpon_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_pm_start_control_ovr_en);
    sensitive << rx1_pm_start_control_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_clksel_en_ovr);
    sensitive << pll_clksel_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_gmode_if_ovr);
    sensitive << rx_rxhs_gmode_if_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_ddl_start_calibration_ovr);
    sensitive << rx3_ddl_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx_des_en_lane1);
    sensitive << rx_des_en_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_txlp_prog);
    sensitive << txlp_prog_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_cal_done_ovr_en);
    sensitive << rx2_cal_done_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_force_lock);
    sensitive << pll_force_lock_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_data_offset_cal_en_ovr);
    sensitive << rx0_rxhs_data_offset_cal_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_vco_cntrl_ovr);
    sensitive << pll_vco_cntrl_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_pm_start_control_ovr);
    sensitive << rx1_pm_start_control_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_compatibility_mode_ovr_en);
    sensitive << rx_rxhs_compatibility_mode_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_del_datalane_ovr_en);
    sensitive << del_datalane_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_pm_start_control_ovr);
    sensitive << rx3_pm_start_control_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_phase_data_ovr_en);
    sensitive << rx3_rxhs_ddl_phase_data_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_txlp_sr_bypass_if);
    sensitive << rx0_txlp_sr_bypass_if_rw << shutdownz_tester;
    SC_METHOD(assign_bist_run_lane1);
    sensitive << bist_run_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_bypass_lane3);
    sensitive << lprxponlp_bypass_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_force_state_bist);
    sensitive << force_state_bist_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_bypass_lane1);
    sensitive << bintpon_bypass_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_ddr_clk_en_if);
    sensitive << rxclk_rxhs_ddr_clk_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_norexttune_done_ovr);
    sensitive << norexttune_done_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_recal_lane0_ovr);
    sensitive << recal_lane0_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_pwron_ovr_en);
    sensitive << pll_pwron_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_bypass_lane0);
    sensitive << lprxponlp_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_bist_run_lane0);
    sensitive << bist_run_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_twakeupulpm);
    sensitive << twakeupulpm_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_pm_start_control_ovr);
    sensitive << rx0_pm_start_control_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_serial_clk_en_ovr);
    sensitive << rxclk_lbck_serial_clk_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_phase_change_p_ovr);
    sensitive << rx3_rxhs_ddl_phase_change_p_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_pm_start_control_ovr_en);
    sensitive << rx2_pm_start_control_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_lock_ovr);
    sensitive << pll_lock_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_deskew_polarity);
    sensitive << deskew_polarity_rw << shutdownz_tester;
    SC_METHOD(assign_ttasure);
    sensitive << ttasure_rw << shutdownz_tester;
    SC_METHOD(assign_rxpontime);
    sensitive << rxpontime_rw << shutdownz_tester;
    SC_METHOD(assign_pg_usert_pat);
    sensitive << pg_usert_pat_rw << shutdownz_tester;
    SC_METHOD(assign_cb_lpibias_en_ovr);
    sensitive << cb_lpibias_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_div_en_ovr);
    sensitive << rxclk_lbck_div_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_serial_en_ovr);
    sensitive << rxclk_lbck_serial_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_bypass_lane3);
    sensitive << hsrxenterm_bypass_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_atb_lptx1200_on_lane0);
    sensitive << atb_lptx1200_on_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_gmode_if_ovr_en);
    sensitive << rx_rxhs_gmode_if_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_phase_data_ovr);
    sensitive << rx3_rxhs_ddl_phase_data_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_gear_shift_ovr_en);
    sensitive << pll_gear_shift_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_prog_ovr_en);
    sensitive << rx_rxhs_prog_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_repeatcal);
    sensitive << rx1_rxhs_repeatcal_rw << shutdownz_tester;
    SC_METHOD(assign_cb_sel_vreflptx_prog);
    sensitive << cb_sel_vreflptx_prog_rw << shutdownz_tester;
    SC_METHOD(assign_atb_sel_lane3);
    sensitive << atb_sel_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_feed_int_clk_ovr);
    sensitive << rx2_rxhs_feed_int_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_lane2);
    sensitive << lprxponulp_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_lane2);
    sensitive << lprxponcd_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_bypass_lane3);
    sensitive << lprxponcd_bypass_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_rx_en200mode_if);
    sensitive << rxclk_rxhs_rx_en200mode_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_clk_from_long_channel_if);
    sensitive << rx2_rxhs_clk_from_long_channel_if_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_serial_clk_en_ovr_en);
    sensitive << rxclk_lbdata_serial_clk_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxtclkmisspd_ovr);
    sensitive << hsrxtclkmisspd_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_if_lane3_tester);
    sensitive << hsrx_en_if_lane3_tester_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_data_ovr);
    sensitive << rx0_rxhs_data_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_start_calibration_ovr);
    sensitive << rx0_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_atb_comp_clk_ovr_en);
    sensitive << cb_atb_comp_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_serial_en_ovr_en);
    sensitive << rxclk_lbck_serial_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_deskew_numedges_update);
    sensitive << deskew_numedges_update_rw << shutdownz_tester;
    SC_METHOD(assign_rx_des_en_bypass_lane3);
    sensitive << rx_des_en_bypass_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_pll_th3);
    sensitive << pll_th3_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_data_eq_if);
    sensitive << rx0_rxhs_data_eq_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_repeatcal);
    sensitive << rx0_rxhs_repeatcal_rw << shutdownz_tester;
    SC_METHOD(assign_setr_max);
    sensitive << setr_max_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_feed_int_clk_ovr);
    sensitive << rxclk_rxhs_feed_int_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_counter_for_des_en_config_if);
    sensitive << counter_for_des_en_config_if_rw << shutdownz_tester;
    SC_METHOD(assign_pg_mode_r);
    sensitive << pg_mode_r_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_if_lane3);
    sensitive << hsrxenterm_if_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_lb_prog_if);
    sensitive << rx0_lb_prog_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_tune_en_if);
    sensitive << rx3_rxhs_ddl_tune_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_bypass_lane2);
    sensitive << lprxponulp_bypass_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_ddl_start_calibration_ovr_en);
    sensitive << rx2_ddl_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_start_calibration_ovr_en);
    sensitive << rx1_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lb_pll_bustie_if);
    sensitive << lb_pll_bustie_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_ddl_finished_ovr_en);
    sensitive << rx3_ddl_finished_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_pull_long_channel_if);
    sensitive << rxclk_rxhs_pull_long_channel_if_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_if_lane0_tester);
    sensitive << hsrx_en_if_lane0_tester_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_ddl_finished_ovr);
    sensitive << rx0_ddl_finished_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_lockdet_mode);
    sensitive << pll_lockdet_mode_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_ddl_start_calibration_ovr_en);
    sensitive << rx1_ddl_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_feed_int_clk_ovr_en);
    sensitive << rxclk_rxhs_feed_int_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_lb_res_cm_en_if);
    sensitive << rx0_lb_res_cm_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_div_en_ovr_en);
    sensitive << rxclk_lbck_div_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_pm_start_control_ovr);
    sensitive << rx2_pm_start_control_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_data_ovr_en);
    sensitive << rx1_rxhs_data_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_th_delay);
    sensitive << pll_th_delay_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_div_en_ovr);
    sensitive << rxclk_lbdata_div_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_hsfreqrange_ovr_en);
    sensitive << hsfreqrange_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_phase_clk_ovr);
    sensitive << rx2_rxhs_ddl_phase_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_pm_start_control_ovr_en);
    sensitive << rx3_pm_start_control_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_if_lane3);
    sensitive << hsrxpon_if_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_cal_done_ovr);
    sensitive << rx2_cal_done_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_bypass_clklane);
    sensitive << bintpon_bypass_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_pll_reset_ovr);
    sensitive << pll_reset_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_feed_int_clk_ovr_en);
    sensitive << rx3_rxhs_feed_int_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_target_state);
    sensitive << target_state_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_lane1);
    sensitive << lprxponlp_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_data_offset_cal_ovr);
    sensitive << rx1_rxhs_data_offset_cal_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_startup_fsm_state);
    sensitive << pll_startup_fsm_state_rw << shutdownz_tester;
    SC_METHOD(assign_cmp_polarity);
    sensitive << cmp_polarity_rw << shutdownz_tester;
    SC_METHOD(assign_bist_run_lane3);
    sensitive << bist_run_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_pm_sample_error_counter);
    sensitive << rx3_pm_sample_error_counter_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_serial_en_ovr_en);
    sensitive << rxclk_lbdata_serial_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lptxena_bypass_lane0);
    sensitive << lptxena_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_bypass_lane1);
    sensitive << hsrxenterm_bypass_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_repeatcal);
    sensitive << rx2_rxhs_repeatcal_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_lane0);
    sensitive << lprxponulp_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_cb_chop_clk_en);
    sensitive << cb_chop_clk_en_rw << shutdownz_tester;
    SC_METHOD(assign_del_datalane_ovr);
    sensitive << del_datalane_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx_des_en_lane0);
    sensitive << rx_des_en_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_lptxenulp_bypass_lane0);
    sensitive << lptxenulp_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_cb_dac_prog_ovr);
    sensitive << cb_dac_prog_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pg_data_polarity);
    sensitive << pg_data_polarity_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_ddr_clk_div_ovr_en);
    sensitive << rxclk_rxhs_ddr_clk_div_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_deskew_jump2steps);
    sensitive << deskew_jump2steps_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_feed_int_clk_ovr);
    sensitive << rx1_rxhs_feed_int_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_lane1);
    sensitive << bintpon_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_lptxenulp_lane0);
    sensitive << lptxenulp_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_recal_lane2_ovr_en);
    sensitive << recal_lane2_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxthssettle_datalanes_ovr);
    sensitive << hsrxthssettle_datalanes_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_norexttune_done_ovr_en);
    sensitive << norexttune_done_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_phase_change_p_ovr_en);
    sensitive << rx1_rxhs_ddl_phase_change_p_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_burnin_cal_en_ovr);
    sensitive << burnin_cal_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_meas_iv);
    sensitive << pll_meas_iv_rw << shutdownz_tester;
    SC_METHOD(assign_cb_sel_vrefcd_lprx);
    sensitive << cb_sel_vrefcd_lprx_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_feed_int_clk_ovr_en);
    sensitive << rx2_rxhs_feed_int_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_lb_res_dn_en_if);
    sensitive << rx1_lb_res_dn_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_pll_n_ovr);
    sensitive << pll_n_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_clklane);
    sensitive << lprxponlp_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_pll_reset_ovr_en);
    sensitive << pll_reset_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_counter_for_des_en_bypass);
    sensitive << counter_for_des_en_bypass_rw << shutdownz_tester;
    SC_METHOD(assign_stability_time);
    sensitive << stability_time_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_bypass_lane3);
    sensitive << bintpon_bypass_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_if_clklane);
    sensitive << hsrxpon_if_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_pll_int_cntrl);
    sensitive << pll_int_cntrl_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_data_ovr);
    sensitive << rx1_rxhs_data_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_lane2);
    sensitive << lprxponlp_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_bypass_clklane);
    sensitive << hsrx_en_bypass_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_ddl_finished_ovr_en);
    sensitive << rx0_ddl_finished_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_skew_back_comp_en_ovr_en);
    sensitive << skew_back_comp_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_bypass_clklane);
    sensitive << lprxponlp_bypass_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_phase_change_p_ovr);
    sensitive << rx0_rxhs_ddl_phase_change_p_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lptxpon_lane0);
    sensitive << lptxpon_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_clklane);
    sensitive << lprxponcd_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_data_ovr_en);
    sensitive << rx0_rxhs_data_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_enable_lanes_ovr_en);
    sensitive << enable_lanes_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_clk_en_ovr_en);
    sensitive << rxclk_lbdata_clk_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_phase_clk_ovr_en);
    sensitive << rx1_rxhs_ddl_phase_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_lane1);
    sensitive << lprxponulp_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_deskew_numedges);
    sensitive << deskew_numedges_rw << shutdownz_tester;
    SC_METHOD(assign_pll_force_fsm_state);
    sensitive << pll_force_fsm_state_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_pm_man_error_latch);
    sensitive << rx0_pm_man_error_latch_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_tune_en_if);
    sensitive << rx1_rxhs_ddl_tune_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_data_offset_cal_en_ovr);
    sensitive << rx1_rxhs_data_offset_cal_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_tolerance);
    sensitive << tolerance_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_pm_mode_r);
    sensitive << rx3_pm_mode_r_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_lb_prog_if);
    sensitive << rx3_lb_prog_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_osc_freq_target_if_ovr_en);
    sensitive << rx0_osc_freq_target_if_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_onpll_ovr);
    sensitive << onpll_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_phase_change_p_ovr);
    sensitive << rx2_rxhs_ddl_phase_change_p_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_bypass_lane1);
    sensitive << lprxponlp_bypass_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_bypass_lane0);
    sensitive << lprxponulp_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_pll_rst_time);
    sensitive << pll_rst_time_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_if_lane2_tester);
    sensitive << hsrx_en_if_lane2_tester_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_lb_res_cm_en_if);
    sensitive << rx1_lb_res_cm_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_burnin_cal_en_ovr_en);
    sensitive << burnin_cal_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_cb_lpibias_en_ovr_en);
    sensitive << cb_lpibias_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_tune_ovr_en);
    sensitive << rx0_rxhs_ddl_tune_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_ddl_osc_freq_target_ovr);
    sensitive << ddl_osc_freq_target_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_serial_clk_en_ovr);
    sensitive << rxclk_lbdata_serial_clk_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_clkouten_right);
    sensitive << pll_clkouten_right_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_osc_res_en_ovr);
    sensitive << rxclk_rxhs_osc_res_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_hsdirect);
    sensitive << hsdirect_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_ddl_start_calibration_ovr_en);
    sensitive << rx3_ddl_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_lb_res_dp_en_if);
    sensitive << rx1_lb_res_dp_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_enable_lanes_ovr);
    sensitive << enable_lanes_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_bypass_lane3);
    sensitive << hsrxpon_bypass_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lb_res_dn_en_if);
    sensitive << rxclk_lb_res_dn_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_hsfreqrange_ovr);
    sensitive << hsfreqrange_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx_des_en_lane2);
    sensitive << rx_des_en_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxtclkmisspd_ovr_en);
    sensitive << hsrxtclkmisspd_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx_ddl_offset_error);
    sensitive << rx_ddl_offset_error_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_ddl_finished_ovr);
    sensitive << rx2_ddl_finished_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_bypass_lane0);
    sensitive << bintpon_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_ttago);
    sensitive << ttago_rw << shutdownz_tester;
    SC_METHOD(assign_deskew_legacyz_ovr_en);
    sensitive << deskew_legacyz_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_osc_freq_target_if_ovr);
    sensitive << rx3_osc_freq_target_if_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_ddr_clk_div_ovr);
    sensitive << rxclk_rxhs_ddr_clk_div_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_offset_comp_threshold);
    sensitive << offset_comp_threshold_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_ddl_clk_out_div);
    sensitive << rx_rxhs_ddl_clk_out_div_rw << shutdownz_tester;
    SC_METHOD(assign_atb_sel_clklane);
    sensitive << atb_sel_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_count_ref);
    sensitive << count_ref_rw << shutdownz_tester;
    SC_METHOD(assign_polarity_lane1);
    sensitive << polarity_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_if_lane0);
    sensitive << hsrxpon_if_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_int_clk_sel);
    sensitive << rxclk_rxhs_int_clk_sel_rw << shutdownz_tester;
    SC_METHOD(assign_pll_gear_shift_ovr);
    sensitive << pll_gear_shift_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_data_offset_cal_ovr_en);
    sensitive << rx2_rxhs_data_offset_cal_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lptxpon_bypass_lane0);
    sensitive << lptxpon_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_lb_res_dn_en_if);
    sensitive << rx3_lb_res_dn_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_cb_cal_repl_ovr);
    sensitive << cb_cal_repl_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_tune_ovr);
    sensitive << rx2_rxhs_ddl_tune_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_vref_mpll_reg_sel);
    sensitive << cb_vref_mpll_reg_sel_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_bypass_clklane);
    sensitive << hsrxpon_bypass_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_pll_boot_from_start);
    sensitive << pll_boot_from_start_rw << shutdownz_tester;
    SC_METHOD(assign_pll_clkouten_left);
    sensitive << pll_clkouten_left_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_phase_clk_ovr);
    sensitive << rx3_rxhs_ddl_phase_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_clk_kill_if);
    sensitive << rxclk_lbdata_clk_kill_if_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_if_lane2);
    sensitive << hsrxenterm_if_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_pll_atb_sense_sel);
    sensitive << pll_atb_sense_sel_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_tune_ovr_en);
    sensitive << rx3_rxhs_ddl_tune_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_deskew_legacyz_ovr);
    sensitive << deskew_legacyz_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_bypass_clklane);
    sensitive << hsrxenterm_bypass_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_tune_en_bypass);
    sensitive << rx0_rxhs_ddl_tune_en_bypass_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_data_offset_cal_en_ovr_en);
    sensitive << rx0_rxhs_data_offset_cal_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_lb_res_dn_en_if);
    sensitive << rx0_lb_res_dn_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_if_clklane);
    sensitive << hsrxenterm_if_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_mastermacro);
    sensitive << mastermacro_rw << shutdownz_tester;
    SC_METHOD(assign_pll_vco_cntrl_ovr_en);
    sensitive << pll_vco_cntrl_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pipe_clean_time);
    sensitive << pipe_clean_time_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_lane0);
    sensitive << bintpon_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_if_lane0);
    sensitive << hsrxenterm_if_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_tune_en_bypass);
    sensitive << rx2_rxhs_ddl_tune_en_bypass_rw << shutdownz_tester;
    SC_METHOD(assign_timebase_ovr);
    sensitive << timebase_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_ddl_finished_ovr);
    sensitive << rx3_ddl_finished_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_clk_kill_if);
    sensitive << rxclk_lbck_clk_kill_if_rw << shutdownz_tester;
    SC_METHOD(assign_tafailsafez);
    sensitive << tafailsafez_rw << shutdownz_tester;
    SC_METHOD(assign_fsm_trigger_ovr);
    sensitive << fsm_trigger_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_atb_comp_clk_ovr);
    sensitive << cb_atb_comp_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_bypass_clklane);
    sensitive << lprxponcd_bypass_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_atb_sel_lane1);
    sensitive << atb_sel_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_pll_lock_sel);
    sensitive << pll_lock_sel_rw << shutdownz_tester;
    SC_METHOD(assign_recal_lane1_ovr_en);
    sensitive << recal_lane1_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lptxena_lane0);
    sensitive << lptxena_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_cb_sel_vref_lprx);
    sensitive << cb_sel_vref_lprx_rw << shutdownz_tester;
    SC_METHOD(assign_deskew_overflow);
    sensitive << deskew_overflow_rw << shutdownz_tester;
    SC_METHOD(assign_setr_typ);
    sensitive << setr_typ_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_if_lane2);
    sensitive << hsrxpon_if_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_osc_freq_target_if_ovr);
    sensitive << rx0_osc_freq_target_if_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_skew_back_comp_en_ovr);
    sensitive << skew_back_comp_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_atb_on_ovr_en);
    sensitive << cb_atb_on_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_data_if);
    sensitive << rxclk_lbck_data_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_data_offset_cal_ovr);
    sensitive << rx3_rxhs_data_offset_cal_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_gear_shift_time);
    sensitive << pll_gear_shift_time_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_clk_from_long_channel_if);
    sensitive << rx3_rxhs_clk_from_long_channel_if_rw << shutdownz_tester;
    SC_METHOD(assign_fsm_trigger_ovr_en);
    sensitive << fsm_trigger_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_cb_atb_sel);
    sensitive << cb_atb_sel_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_phase_clk_ovr_en);
    sensitive << rx2_rxhs_ddl_phase_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_lock_det_on_ovr_en);
    sensitive << pll_lock_det_on_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_data_offset_cal_en_ovr);
    sensitive << rx3_rxhs_data_offset_cal_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx_cb_lb_en_rext_if);
    sensitive << rx_cb_lb_en_rext_if_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_bypass_lane3);
    sensitive << hsrx_en_bypass_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_skew_mux_sel);
    sensitive << skew_mux_sel_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_ddl_start_calibration_ovr_en);
    sensitive << rx0_ddl_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_start_calibration_ovr_en);
    sensitive << rx3_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_tune_ovr_en);
    sensitive << rx2_rxhs_ddl_tune_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_start_calibration_ovr);
    sensitive << rxclk_rxhs_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_feed_int_clk_ovr);
    sensitive << rx0_rxhs_feed_int_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_bg_en_ovr);
    sensitive << cb_bg_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_sel_chop_clk);
    sensitive << cb_sel_chop_clk_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_start_calibration_ovr);
    sensitive << rxclk_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_data_offset_cal_en_ovr_en);
    sensitive << rx2_rxhs_data_offset_cal_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_data_retiming_out_swap_sel);
    sensitive << rx0_data_retiming_out_swap_sel_rw << shutdownz_tester;
    SC_METHOD(assign_tclk_miss_opt);
    sensitive << tclk_miss_opt_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_serial_en_ovr);
    sensitive << rxclk_lbdata_serial_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_pwron_ovr);
    sensitive << pll_pwron_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_polarity_lane0);
    sensitive << polarity_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_start_calibration_ovr);
    sensitive << rx1_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_lb_prog_if);
    sensitive << rx1_lb_prog_if_rw << shutdownz_tester;
    SC_METHOD(assign_cb_ical_x4_ovr);
    sensitive << cb_ical_x4_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_start_calibration_ovr_en);
    sensitive << rxclk_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_lane3);
    sensitive << bintpon_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_data_eq_if);
    sensitive << rx2_rxhs_data_eq_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_compatibility_mode_ovr);
    sensitive << rx_rxhs_compatibility_mode_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_pm_sample_error_counter);
    sensitive << rx0_pm_sample_error_counter_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_start_calibration_ovr_en);
    sensitive << rx0_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_th1);
    sensitive << pll_th1_rw << shutdownz_tester;
    SC_METHOD(assign_cb_cal_en_ovr);
    sensitive << cb_cal_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_clklane);
    sensitive << bintpon_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_if_clklane_tester);
    sensitive << hsrx_en_if_clklane_tester_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_pm_mode_r);
    sensitive << rx1_pm_mode_r_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_data_ovr);
    sensitive << rx2_rxhs_data_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_bintpon_bypass_lane2);
    sensitive << bintpon_bypass_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_atb_sel_lane0);
    sensitive << atb_sel_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_pll_opmode);
    sensitive << pll_opmode_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_osc_freq_target_if_ovr);
    sensitive << rx2_osc_freq_target_if_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_txdindlytime);
    sensitive << txdindlytime_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_osc_freq_target_if_ovr);
    sensitive << rx1_osc_freq_target_if_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_lock_det_time);
    sensitive << pll_lock_det_time_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_clk_offset_cal_ovr);
    sensitive << rxclk_rxhs_clk_offset_cal_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_data_ovr_en);
    sensitive << rx3_rxhs_data_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_data_offset_cal_ovr);
    sensitive << rx2_rxhs_data_offset_cal_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pg_bist_error);
    sensitive << pg_bist_error_rw << shutdownz_tester;
    SC_METHOD(assign_cb_en_45_ohm);
    sensitive << cb_en_45_ohm_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_lb_res_cm_en_if);
    sensitive << rx2_lb_res_cm_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_phase_clk_ovr);
    sensitive << rx1_rxhs_ddl_phase_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_lane0);
    sensitive << lprxponcd_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_phase_data_ovr_en);
    sensitive << rx0_rxhs_ddl_phase_data_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_lb_res_dp_en_if);
    sensitive << rx3_lb_res_dp_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_pm_man_error_latch);
    sensitive << rx1_pm_man_error_latch_rw << shutdownz_tester;
    SC_METHOD(assign_cb_cal_repl_ovr_en);
    sensitive << cb_cal_repl_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_ddl_finished_ovr_en);
    sensitive << rx2_ddl_finished_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_bypass_lane0);
    sensitive << hsrx_en_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_ddl_finished_ovr_en);
    sensitive << rx1_ddl_finished_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_tune_en_if);
    sensitive << rx0_rxhs_ddl_tune_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_data_eq_if);
    sensitive << rx1_rxhs_data_eq_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_data_ovr_en);
    sensitive << rx2_rxhs_data_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_phase_data_ovr_en);
    sensitive << rx1_rxhs_ddl_phase_data_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx_des_en_bypass_lane1);
    sensitive << rx_des_en_bypass_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_cal_done_ovr);
    sensitive << rx0_cal_done_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lb_res_dp_en_if);
    sensitive << rxclk_lb_res_dp_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_tclk_miss_div2_ovr_en);
    sensitive << tclk_miss_div2_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_polarity_lane3);
    sensitive << polarity_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_cal_done_ovr_en);
    sensitive << rx0_cal_done_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_setr_ovr_en);
    sensitive << rx_rxhs_setr_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_polarity_clklane);
    sensitive << polarity_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_number_of_ready_count);
    sensitive << number_of_ready_count_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_data_ovr);
    sensitive << rx3_rxhs_data_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_bypass_lane0);
    sensitive << hsrxpon_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_div_en_ovr_en);
    sensitive << rxclk_lbdata_div_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_bypass_ddltunning_machine);
    sensitive << bypass_ddltunning_machine_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lb_res_cm_en_if);
    sensitive << rxclk_lb_res_cm_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_lb_prog_if);
    sensitive << rx2_lb_prog_if_rw << shutdownz_tester;
    SC_METHOD(assign_pll_lock_ovr_en);
    sensitive << pll_lock_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_data_retiming_out_swap_sel);
    sensitive << rx3_data_retiming_out_swap_sel_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxenterm_if_lane1);
    sensitive << hsrxenterm_if_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_clk_from_long_channel_if);
    sensitive << rx0_rxhs_clk_from_long_channel_if_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_caldone_ovr);
    sensitive << rxclk_caldone_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_data_offset_cal_en_ovr_en);
    sensitive << rx3_rxhs_data_offset_cal_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_recal_lane3_ovr_en);
    sensitive << recal_lane3_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_lb_res_dp_en_if);
    sensitive << rx2_lb_res_dp_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_pm_mode_r);
    sensitive << rx0_pm_mode_r_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_start_calibration_ovr);
    sensitive << rx2_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_sel_v400_prog);
    sensitive << cb_sel_v400_prog_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_if_lane1);
    sensitive << hsrxpon_if_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_bist_run_lane2);
    sensitive << bist_run_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_phase_change_p_ovr_en);
    sensitive << rx3_rxhs_ddl_phase_change_p_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_noalign_error);
    sensitive << noalign_error_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_clklane);
    sensitive << lprxponulp_clklane_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_osc_freq_target_if_ovr_en);
    sensitive << rx2_osc_freq_target_if_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_skew_mux_run);
    sensitive << skew_mux_run_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_pm_man_error_latch);
    sensitive << rx2_pm_man_error_latch_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lb_sel_ovr);
    sensitive << rxclk_lb_sel_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_bypass_lane2);
    sensitive << lprxponlp_bypass_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_onpll_ovr_en);
    sensitive << onpll_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_clk_en_ovr_en);
    sensitive << rxclk_lbck_clk_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_ddl_osc_freq_target_ovr_en);
    sensitive << ddl_osc_freq_target_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_data_retiming_out_swap_sel);
    sensitive << rx2_data_retiming_out_swap_sel_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_bypass_lane3);
    sensitive << lprxponulp_bypass_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_pm_sample_error_counter);
    sensitive << rx2_pm_sample_error_counter_rw << shutdownz_tester;
    SC_METHOD(assign_bypass_offset_machine);
    sensitive << bypass_offset_machine_rw << shutdownz_tester;
    SC_METHOD(assign_pll_prop_cntrl);
    sensitive << pll_prop_cntrl_rw << shutdownz_tester;
    SC_METHOD(assign_rx_cb_ibias_en_if);
    sensitive << rx_cb_ibias_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_lane1);
    sensitive << lprxponcd_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_pll_m_ovr);
    sensitive << pll_m_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_recal_lane0_ovr_en);
    sensitive << recal_lane0_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_cb_atb_ext_con);
    sensitive << cb_atb_ext_con_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_cal_done_ovr_en);
    sensitive << rx3_cal_done_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_lock_det_on_ovr);
    sensitive << pll_lock_det_on_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_adc_done_clear);
    sensitive << adc_done_clear_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_cal_done_ovr_en);
    sensitive << rx1_cal_done_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_bypass_lane2);
    sensitive << hsrxpon_bypass_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_phase_clk_ovr_en);
    sensitive << rx0_rxhs_ddl_phase_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_force_state);
    sensitive << force_state_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_clk_offset_cal_ovr_en);
    sensitive << rxclk_rxhs_clk_offset_cal_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_phase_data_ovr_en);
    sensitive << rx2_rxhs_ddl_phase_data_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_if_lane1_tester);
    sensitive << hsrx_en_if_lane1_tester_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_ddr_clk_en_bypass);
    sensitive << rxclk_rxhs_ddr_clk_en_bypass_rw << shutdownz_tester;
    SC_METHOD(assign_timeinitbg);
    sensitive << timeinitbg_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_caldone_ovr_en);
    sensitive << rxclk_caldone_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_pm_sample_error_counter);
    sensitive << rx1_pm_sample_error_counter_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_bypass_lane1);
    sensitive << lprxponcd_bypass_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_ddr_clk_sel_if);
    sensitive << rxclk_rxhs_ddr_clk_sel_if_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxlp_bias_prog);
    sensitive << rx_rxlp_bias_prog_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_lane3);
    sensitive << lprxponulp_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_repeatcal);
    sensitive << rx3_rxhs_repeatcal_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_lane3);
    sensitive << lprxponcd_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_tune_ovr);
    sensitive << rx1_rxhs_ddl_tune_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_clk_en_lanes_tester);
    sensitive << clk_en_lanes_tester_rw << shutdownz_tester;
    SC_METHOD(assign_pll_tstplldig);
    sensitive << pll_tstplldig_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_phase_clk_ovr_en);
    sensitive << rx3_rxhs_ddl_phase_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_data_offset_cal_en_ovr_en);
    sensitive << rx1_rxhs_data_offset_cal_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_data_offset_cal_ovr);
    sensitive << rx0_rxhs_data_offset_cal_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_pll_gmp_cntrl);
    sensitive << pll_gmp_cntrl_rw << shutdownz_tester;
    SC_METHOD(assign_noext_burnin_res_cal);
    sensitive << noext_burnin_res_cal_rw << shutdownz_tester;
    SC_METHOD(assign_pll_th2);
    sensitive << pll_th2_rw << shutdownz_tester;
    SC_METHOD(assign_atb_sel_lane2);
    sensitive << atb_sel_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_tune_ovr);
    sensitive << rx3_rxhs_ddl_tune_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_ddl_phase_data_ovr);
    sensitive << rx2_rxhs_ddl_phase_data_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_phase_data_ovr);
    sensitive << rx1_rxhs_ddl_phase_data_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_cal_en_ovr_en);
    sensitive << cb_cal_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_timebase_ovr_en);
    sensitive << timebase_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_osc_freq_target_if_ovr_en);
    sensitive << rx3_osc_freq_target_if_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_lb_res_cm_en_if);
    sensitive << rx3_lb_res_cm_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_hsrx_en_bypass_lane2);
    sensitive << hsrx_en_bypass_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_pll_mpll_prog);
    sensitive << pll_mpll_prog_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponulp_bypass_lane1);
    sensitive << lprxponulp_bypass_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_lane0);
    sensitive << lprxponlp_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_pll_cpbias_cntrl);
    sensitive << pll_cpbias_cntrl_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_prog_ovr);
    sensitive << rx_rxhs_prog_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_bist_manual);
    sensitive << bist_manual_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_lb_res_dn_en_if);
    sensitive << rx2_lb_res_dn_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_adc_en);
    sensitive << adc_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_feed_int_clk_ovr);
    sensitive << rx3_rxhs_feed_int_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponlp_lane3);
    sensitive << lprxponlp_lane3_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_pm_mode_r);
    sensitive << rx2_pm_mode_r_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_osc_freq_target_if_ovr_en);
    sensitive << rx1_osc_freq_target_if_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lptxdin_bypass_lane0);
    sensitive << lptxdin_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_phase_change_p_ovr_en);
    sensitive << rx0_rxhs_ddl_phase_change_p_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_lptxenpulldwn_lane0);
    sensitive << lptxenpulldwn_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_clk_to_long_channel_if);
    sensitive << rxclk_rxhs_clk_to_long_channel_if_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbck_clk_en_ovr);
    sensitive << rxclk_lbck_clk_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_atb_vbe_sel);
    sensitive << cb_atb_vbe_sel_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_data_offset_cal_ovr_en);
    sensitive << rx3_rxhs_data_offset_cal_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pg_reg_err_r);
    sensitive << pg_reg_err_r_rw << shutdownz_tester;
    SC_METHOD(assign_deskew_latency);
    sensitive << deskew_latency_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_ddl_start_calibration_ovr);
    sensitive << rx0_ddl_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_comp_after_strobes);
    sensitive << comp_after_strobes_rw << shutdownz_tester;
    SC_METHOD(assign_rx_des_en_bypass_lane2);
    sensitive << rx_des_en_bypass_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_pll_clksel_en_ovr_en);
    sensitive << pll_clksel_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_ddl_start_calibration_ovr);
    sensitive << rx1_ddl_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_phase_change_p_ovr);
    sensitive << rx1_rxhs_ddl_phase_change_p_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_feed_int_clk_ovr_en);
    sensitive << rx1_rxhs_feed_int_clk_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_data_offset_cal_ovr_en);
    sensitive << rx1_rxhs_data_offset_cal_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_pll_n_ovr_en);
    sensitive << pll_n_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx_des_en_bypass_lane0);
    sensitive << rx_des_en_bypass_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_hsrxpon_bypass_lane1);
    sensitive << hsrxpon_bypass_lane1_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_rxhs_ddl_tune_en_bypass);
    sensitive << rx3_rxhs_ddl_tune_en_bypass_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_rxhs_data_offset_cal_en_ovr);
    sensitive << rx2_rxhs_data_offset_cal_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lptxdin_lane0);
    sensitive << lptxdin_lane0_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_cal_done_ovr);
    sensitive << rx1_cal_done_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_data_offset_cal_ovr_en);
    sensitive << rx0_rxhs_data_offset_cal_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_recal_lane3_ovr);
    sensitive << recal_lane3_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_phase_clk_ovr);
    sensitive << rx0_rxhs_ddl_phase_clk_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_lprxponcd_bypass_lane2);
    sensitive << lprxponcd_bypass_lane2_rw << shutdownz_tester;
    SC_METHOD(assign_cb_bg_en_ovr_en);
    sensitive << cb_bg_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_osc_pon_ovr);
    sensitive << rxclk_rxhs_osc_pon_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_start_calibration_ovr_en);
    sensitive << rxclk_rxhs_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_lbdata_clk_en_ovr);
    sensitive << rxclk_lbdata_clk_en_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_on_ovr);
    sensitive << cb_on_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_cb_dac_prog_ovr_en);
    sensitive << cb_dac_prog_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_lb_res_dp_en_if);
    sensitive << rx0_lb_res_dp_en_if_rw << shutdownz_tester;
    SC_METHOD(assign_rxclk_rxhs_osc_res_en_ovr_en);
    sensitive << rxclk_rxhs_osc_res_en_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx0_rxhs_ddl_tune_ovr);
    sensitive << rx0_rxhs_ddl_tune_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx3_start_calibration_ovr);
    sensitive << rx3_start_calibration_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_rx2_start_calibration_ovr_en);
    sensitive << rx2_start_calibration_ovr_en_rw << shutdownz_tester;
    SC_METHOD(assign_rx1_rxhs_ddl_tune_en_bypass);
    sensitive << rx1_rxhs_ddl_tune_en_bypass_rw << shutdownz_tester;
    SC_METHOD(assign_rx_rxhs_setr_ovr);
    sensitive << rx_rxhs_setr_ovr_rw << shutdownz_tester;
    SC_METHOD(assign_testdouts_0);
    sensitive << xaddr << hsfreqrange_ovr_en_rw << target_state_rw << force_state_rw;
    SC_METHOD(assign_testdouts_0_2);
    sensitive << xaddr << timebase_ovr_en_rw << hsfreqrange_ovr_rw;
    SC_METHOD(assign_testdouts_0_3);
    sensitive << xaddr << timebase_ovr_rw;
    SC_METHOD(assign_testdouts_0_4);
    sensitive << xaddr << cmp_polarity_rw << noext_burnin_res_cal_rw << hsdirect_rw;
    SC_METHOD(assign_testdouts_0_5);
    sensitive << xaddr << hsrxtclkmisspd_ovr_en_rw << hsrxtclkmisspd_ovr_rw << tclk_miss_opt_rw << number_of_ready_count_rw;
    SC_METHOD(assign_testdouts_0_6);
    sensitive << xaddr << offset_comp_threshold_rw;
    SC_METHOD(assign_testdouts_0_7);
    sensitive << xaddr << tolerance_rw;
    SC_METHOD(assign_testdouts_0_8);
    sensitive << xaddr << tclk_miss_div2_ovr_en_rw << tclk_miss_div2_ovr_rw << deskew_polarity_rw << deskew_latency_rw << skew_mux_sel_rw << skew_mux_run_rw;
    SC_METHOD(assign_testdouts_0_9);
    sensitive << xaddr << tafailsafez_rw << aligner_dk_cnf_rw << noalign_error_rw << deskew_jump2steps_rw << deskew_overflow_rw << deskew_numedges_update_rw;
    SC_METHOD(assign_testdouts_0_10);
    sensitive << xaddr << deskew_numedges_rw;
    SC_METHOD(assign_testdouts_0_11);
    sensitive << xaddr << hsdirect_if << cb_det_supply << state_main;
    SC_METHOD(assign_testdouts_0_12);
    sensitive << xaddr << hsfreqrange;
    SC_METHOD(assign_testdouts_0_13);
    sensitive << xaddr << timebase;
    SC_METHOD(assign_testdouts_0_14);
    sensitive << xaddr << ulpsactivenot_global << txulpsexit_global << cb_cal_en_ifpowerctrl << lptxenpulldwn_ifpowerctrl << lptxdin_ifpowerctrl << lptxena_ifpowerctrl << lptxpon_ifpowerctrl << bintpon_ifpowerctrl;
    SC_METHOD(assign_testdouts_0_15);
    sensitive << xaddr << cont_data;
    SC_METHOD(assign_testdouts_0_16);
    sensitive << xaddr << cont_data;
    SC_METHOD(assign_testdouts_0_17);
    sensitive << xaddr << rx_rxhs_prog;
    SC_METHOD(assign_testdouts_0_18);
    sensitive << xaddr << rx_ddl_offset_error_rw;
    SC_METHOD(assign_testdouts_0_19);
    sensitive << xaddr << rx_ddl_offset_error_rw;
    SC_METHOD(assign_testdouts_0_20);
    sensitive << xaddr << rx_rxhs_ddl_clk_out_div_rw;
    SC_METHOD(assign_testdouts_0_21);
    sensitive << xaddr << timeinitbg_rw;
    SC_METHOD(assign_testdouts_0_22);
    sensitive << xaddr << timeinitbg_rw;
    SC_METHOD(assign_testdouts_0_23);
    sensitive << xaddr << ttago_rw;
    SC_METHOD(assign_testdouts_0_24);
    sensitive << xaddr << ttasure_rw;
    SC_METHOD(assign_testdouts_0_25);
    sensitive << xaddr << ttawait_rw;
    SC_METHOD(assign_testdouts_0_26);
    sensitive << xaddr << twakeupulpm_rw;
    SC_METHOD(assign_testdouts_0_27);
    sensitive << xaddr << twakeupulpm_rw;
    SC_METHOD(assign_testdouts_0_28);
    sensitive << xaddr << txdindlytime_rw;
    SC_METHOD(assign_testdouts_0_29);
    sensitive << xaddr << txdindlytime_rw;
    SC_METHOD(assign_testdouts_0_30);
    sensitive << xaddr << txstarttime_rw;
    SC_METHOD(assign_testdouts_0_31);
    sensitive << xaddr << txstarttime_rw;
    SC_METHOD(assign_testdouts_0_32);
    sensitive << xaddr << rxpontime_rw;
    SC_METHOD(assign_testdouts_0_33);
    sensitive << xaddr << hsrxthssettle_datalanes;
    SC_METHOD(assign_testdouts_0_34);
    sensitive << xaddr << hsrxpon_clklane_ifpowerctrl << hsrxencal_lane_offset_ifpowerctrl << clk_en_lanes << des_en_for_datalane_offset_cal_ifpowerctrl << rxlane_feed_int_clk_ifpowerctrl << rxclk_rxhs_feed_int_clk_if << rxclk_feed_int_clk_ifpowerctrl << burnin_cal_en_if;
    SC_METHOD(assign_testdouts_0_35);
    sensitive << xaddr << skew_back_comp_en << skew_back_comp_en_ifpowerctrl << perform_skew << rx_rxhs_compatibility_mode << global_ddltunning_done << ddltunning_en_ifpowerctrl << global_offset_cal_done << hsrxpon_datalane_ifpowerctrl;
    SC_METHOD(assign_testdouts_0_36);
    sensitive << xaddr << deskew_legacyz << enable_lanes << rx_rxhs_gmode << align_error << global_skew_cal_done;
    SC_METHOD(assign_testdouts_0_37);
    sensitive << xaddr << counter_for_des_en_if;
    SC_METHOD(assign_testdouts_0_38);
    sensitive << xaddr << hsfreqrange_top;
    SC_METHOD(assign_testdouts_0_39);
    sensitive << xaddr << rx_rxhs_compatibility_mode_ovr_en_rw << bypass_skew_machine_rw << skew_back_comp_en_ovr_rw << skew_back_comp_en_ovr_en_rw << bypass_ddltunning_machine_rw << bypass_offset_machine_rw << clk_en_lanes_tester_rw << clk_en_lanes_bypass_rw;
    SC_METHOD(assign_testdouts_0_40);
    sensitive << xaddr << deskew_legacyz_ovr_en_rw << deskew_legacyz_ovr_rw << enable_lanes_ovr_rw << enable_lanes_ovr_en_rw << rxclk_rxhs_ddr_clk_en_bypass_rw << rx_rxhs_compatibility_mode_ovr_rw;
    SC_METHOD(assign_testdouts_0_41);
    sensitive << xaddr << ddl_osc_freq_target_ovr_rw;
    SC_METHOD(assign_testdouts_0_42);
    sensitive << xaddr << ddl_osc_freq_target_ovr_rw;
    SC_METHOD(assign_testdouts_0_43);
    sensitive << xaddr << counter_for_des_en_config_if_rw << rx_rxhs_gmode_if_ovr_rw << rx_rxhs_gmode_if_ovr_en_rw << ddl_osc_freq_target_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_44);
    sensitive << xaddr << counter_for_des_en_bypass_rw;
    SC_METHOD(assign_testdouts_0_45);
    sensitive << xaddr << hsrxthssettle_ovr_en_datalanes_rw << hsrxthssettle_datalanes_ovr_rw;
    SC_METHOD(assign_testdouts_0_46);
    sensitive << xaddr << pg_mode_r_rw << pg_bist_error_rw << pg_data_polarity_rw << pg_data_swap_rw << pg_reg_err_r_rw;
    SC_METHOD(assign_testdouts_0_47);
    sensitive << xaddr << pg_usert_pat_rw;
    SC_METHOD(assign_testdouts_0_48);
    sensitive << xaddr << rxclk_lbck_data_if_rw;
    SC_METHOD(assign_testdouts_0_49);
    sensitive << xaddr << bist_run_lane3_rw << bist_run_lane2_rw << bist_run_lane1_rw << bist_run_lane0_rw << bist_manual_rw << comp_after_strobes_rw;
    SC_METHOD(assign_testdouts_0_50);
    sensitive << xaddr << rxclk_lbck_clk_kill_if_rw << rxclk_lbdata_clk_kill_if_rw << force_state_bist_rw << target_state_bist_rw;
    SC_METHOD(assign_testdouts_0_51);
    sensitive << xaddr << rxclk_lbck_serial_en_ovr_en_rw << rxclk_lbck_serial_en_ovr_rw << rxclk_lbck_serial_clk_en_ovr_en_rw << rxclk_lbck_serial_clk_en_ovr_rw << rxclk_lbck_div_en_ovr_en_rw << rxclk_lbck_div_en_ovr_rw << rxclk_lbck_clk_en_ovr_en_rw << rxclk_lbck_clk_en_ovr_rw;
    SC_METHOD(assign_testdouts_0_52);
    sensitive << xaddr << rxclk_lbdata_serial_en_ovr_en_rw << rxclk_lbdata_serial_en_ovr_rw << rxclk_lbdata_serial_clk_en_ovr_en_rw << rxclk_lbdata_serial_clk_en_ovr_rw << rxclk_lbdata_div_en_ovr_en_rw << rxclk_lbdata_div_en_ovr_rw << rxclk_lbdata_clk_en_ovr_en_rw << rxclk_lbdata_clk_en_ovr_rw;
    SC_METHOD(assign_testdouts_0_53);
    sensitive << xaddr << del_datalane_ovr_rw;
    SC_METHOD(assign_testdouts_0_54);
    sensitive << xaddr << rx_rxhs_prog_ovr_en_rw << rx_rxhs_prog_ovr_rw << rxclk_lb_sel_ovr_en_rw << rxclk_lb_sel_ovr_rw << fsm_trigger_ovr_en_rw << fsm_trigger_ovr_rw << del_datalane_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_55);
    sensitive << xaddr << pipe_clean_time_rw;
    SC_METHOD(assign_testdouts_0_56);
    sensitive << xaddr << rx3_error_latch_if << rx2_error_latch_if << rx1_error_latch_if << rx0_error_latch_if << pg_sel << pg_pat_strobe << biston_lanes;
    SC_METHOD(assign_testdouts_0_57);
    sensitive << xaddr << rxclk_lbck_clk_en_if << rxclk_lbck_div_en_if << rxclk_lbck_serial_clk_en_if << rxclk_lbck_serial_en_if << rxclk_lbdata_clk_en_if << rxclk_lbdata_div_en_if << rxclk_lbdata_serial_clk_en_if << rxclk_lbdata_serial_en_if;
    SC_METHOD(assign_testdouts_0_58);
    sensitive << xaddr << error_gating_out << bist_state << fsm_trigger << rxclk_lb_sel_if;
    SC_METHOD(assign_testdouts_0_59);
    sensitive << xaddr << del_datalane;
    SC_METHOD(assign_testdouts_0_60);
    sensitive << xaddr << bistok << bistdone;
    SC_METHOD(assign_testdouts_0_61);
    sensitive << xaddr << mastermacro_rw;
    SC_METHOD(assign_testdouts_0_62);
    sensitive << xaddr << rxclk_rxhs_rx_en_in << rxclk_rxhs_rx_en_out << rxclk_rxhs_rx_pon_in << rxclk_rxhs_rx_pon_out << rxclk_rxhs_rx_enlogic_in << rxclk_rxhs_rx_enlogic_out << rxclk_rxhs_clk_en_in << rxclk_rxhs_clk_en_out;
    SC_METHOD(assign_testdouts_0_63);
    sensitive << xaddr << rxclk_rxhs_feed_int_clk_in << rxclk_rxhs_feed_int_clk_out << rx_rxhs_prog_in << rx_rxhs_prog_out << ulpsactivenotclk_in;
    SC_METHOD(assign_testdouts_0_64);
    sensitive << xaddr << launch_des_en_in << launch_des_en_out;
    SC_METHOD(assign_testdouts_0_65);
    sensitive << xaddr << lb_pll_bustie_if_rw << pll_boot_from_start_rw << pll_clksel_en_ovr_rw << pll_clksel_en_ovr_en_rw << pll_atb_sense_sel_rw << onpll_ovr_rw << onpll_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_66);
    sensitive << xaddr << pll_force_fsm_state_rw << pll_cpbias_cntrl_rw;
    SC_METHOD(assign_testdouts_0_67);
    sensitive << xaddr << pll_gear_shift_ovr_rw << pll_gear_shift_ovr_en_rw << pll_force_lock_rw;
    SC_METHOD(assign_testdouts_0_68);
    sensitive << xaddr << pll_gear_shift_time_rw;
    SC_METHOD(assign_testdouts_0_69);
    sensitive << xaddr << pll_gear_shift_time_rw;
    SC_METHOD(assign_testdouts_0_70);
    sensitive << xaddr << pll_int_cntrl_rw << pll_gmp_cntrl_rw;
    SC_METHOD(assign_testdouts_0_71);
    sensitive << xaddr << pll_lock_det_on_ovr_rw << pll_lock_det_on_ovr_en_rw << pll_lockdet_mode_rw;
    SC_METHOD(assign_testdouts_0_72);
    sensitive << xaddr << pll_lock_det_time_rw;
    SC_METHOD(assign_testdouts_0_73);
    sensitive << xaddr << pll_lock_det_time_rw;
    SC_METHOD(assign_testdouts_0_74);
    sensitive << xaddr << pll_lock_sel_rw << pll_lock_ovr_rw << pll_lock_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_75);
    sensitive << xaddr << pll_meas_iv_rw;
    SC_METHOD(assign_testdouts_0_76);
    sensitive << xaddr << pll_meas_iv_rw;
    SC_METHOD(assign_testdouts_0_77);
    sensitive << xaddr << pll_meas_iv_rw;
    SC_METHOD(assign_testdouts_0_78);
    sensitive << xaddr << pll_mpll_prog_rw;
    SC_METHOD(assign_testdouts_0_79);
    sensitive << xaddr << pll_mpll_prog_rw;
    SC_METHOD(assign_testdouts_0_80);
    sensitive << xaddr << pll_mpll_prog_rw;
    SC_METHOD(assign_testdouts_0_81);
    sensitive << xaddr << pll_opmode_rw;
    SC_METHOD(assign_testdouts_0_82);
    sensitive << xaddr << pll_pwron_ovr_rw << pll_pwron_ovr_en_rw << pll_prop_cntrl_rw;
    SC_METHOD(assign_testdouts_0_83);
    sensitive << xaddr << pll_reset_ovr_rw << pll_reset_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_84);
    sensitive << xaddr << pll_rst_time_rw;
    SC_METHOD(assign_testdouts_0_85);
    sensitive << xaddr << pll_rst_time_rw;
    SC_METHOD(assign_testdouts_0_86);
    sensitive << xaddr << pll_startup_fsm_state_rw;
    SC_METHOD(assign_testdouts_0_87);
    sensitive << xaddr << pll_th1_rw;
    SC_METHOD(assign_testdouts_0_88);
    sensitive << xaddr << pll_th1_rw;
    SC_METHOD(assign_testdouts_0_89);
    sensitive << xaddr << pll_th2_rw;
    SC_METHOD(assign_testdouts_0_90);
    sensitive << xaddr << pll_th3_rw;
    SC_METHOD(assign_testdouts_0_91);
    sensitive << xaddr << pll_th_delay_rw;
    SC_METHOD(assign_testdouts_0_92);
    sensitive << xaddr << pll_n_ovr_en_rw << pll_n_ovr_rw << pll_tstplldig_rw;
    SC_METHOD(assign_testdouts_0_93);
    sensitive << xaddr << pll_m_ovr_rw;
    SC_METHOD(assign_testdouts_0_94);
    sensitive << xaddr << pll_m_ovr_rw;
    SC_METHOD(assign_testdouts_0_95);
    sensitive << xaddr << pll_vco_cntrl_ovr_en_rw << pll_vco_cntrl_ovr_rw << pll_m_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_96);
    sensitive << xaddr << pll_clkouten_right_rw << pll_clkouten_left_rw;
    SC_METHOD(assign_testdouts_0_97);
    sensitive << xaddr << pll_vpl_det << pll_clksel_en << pll_lock << pll_lock_det_on << pll_gear_shift << onpll;
    SC_METHOD(assign_testdouts_0_98);
    sensitive << xaddr << pll_m;
    SC_METHOD(assign_testdouts_0_99);
    sensitive << xaddr << pll_m;
    SC_METHOD(assign_testdouts_0_100);
    sensitive << xaddr << pll_startup_done << pll_reset << pll_pwron << pll_n;
    SC_METHOD(assign_testdouts_0_101);
    sensitive << xaddr << pll_lock_int << pll_testlock << pll_vco_cntrl;
    SC_METHOD(assign_testdouts_0_102);
    sensitive << xaddr << cb_sel_vrefcd_lprx_rw << cb_sel_v400_prog_rw << cb_sel_chop_clk_rw << cb_chop_clk_en_rw;
    SC_METHOD(assign_testdouts_0_103);
    sensitive << xaddr << cb_atb_vbe_sel_rw << txlp_prog_rw << cb_sel_vreflptx_prog_rw << cb_sel_vref_lprx_rw;
    SC_METHOD(assign_testdouts_0_104);
    sensitive << xaddr << rx_rxlp_bias_prog_rw << rx_cb_pre_iref_on_rw << cb_atb_ext_con_rw << cb_atb_sel_rw;
    SC_METHOD(assign_testdouts_0_105);
    sensitive << xaddr << cb_lpibias_en_ovr_en_rw << cb_bg_en_ovr_rw << cb_bg_en_ovr_en_rw << cb_atb_comp_clk_ovr_rw << cb_atb_comp_clk_ovr_en_rw << cb_vref_mpll_reg_sel_rw;
    SC_METHOD(assign_testdouts_0_106);
    sensitive << xaddr << cb_atb_on_ovr_en_rw << cb_on_ovr_rw << cb_on_ovr_en_rw << cb_lpibias_en_ovr_rw;
    SC_METHOD(assign_testdouts_0_107);
    sensitive << xaddr << cb_atb_on_ovr_rw;
    SC_METHOD(assign_testdouts_0_108);
    sensitive << xaddr << rx_cb_lb_en_rext_if_rw << rx_cb_lb_setrx_if_rw << rx_cb_ibias_en_if_rw;
    SC_METHOD(assign_testdouts_0_109);
    sensitive << xaddr << cb_on << cb_lpibias_en << cb_ical_x4 << cb_bg_en << cb_atb_on << cb_atb_comp_clk;
    SC_METHOD(assign_testdouts_0_110);
    sensitive << xaddr << adc_done_clear_rw << adc_en_rw;
    SC_METHOD(assign_testdouts_0_111);
    sensitive << xaddr << cb_dac_prog_ovr_rw;
    SC_METHOD(assign_testdouts_0_112);
    sensitive << xaddr << cb_dac_prog_ovr_rw;
    SC_METHOD(assign_testdouts_0_113);
    sensitive << xaddr << cb_dac_prog_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_114);
    sensitive << xaddr << adc_done;
    SC_METHOD(assign_testdouts_0_115);
    sensitive << xaddr << cb_dac_prog;
    SC_METHOD(assign_testdouts_0_116);
    sensitive << xaddr << cb_dac_prog;
    SC_METHOD(assign_testdouts_0_117);
    sensitive << xaddr << cb_cal_repl_ovr_rw << cb_cal_en_ovr_rw << cb_cal_en_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_118);
    sensitive << xaddr << cb_en_45_ohm_rw << cb_cal_repl_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_119);
    sensitive << xaddr << cb_ical_x4_ovr_rw << cb_ical_x4_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_120);
    sensitive << xaddr << cb_cal_repl << cb_cal_en;
    SC_METHOD(assign_testdouts_0_121);
    sensitive << xaddr << rescal_done << rescal_en << cb_comp_out;
    SC_METHOD(assign_testdouts_0_122);
    sensitive << xaddr << rescal_error;
    SC_METHOD(assign_testdouts_0_123);
    sensitive << xaddr << count_ref_rw;
    SC_METHOD(assign_testdouts_0_124);
    sensitive << xaddr << count_ref_rw;
    SC_METHOD(assign_testdouts_0_125);
    sensitive << xaddr << setr_min_rw << setr_max_rw;
    SC_METHOD(assign_testdouts_0_126);
    sensitive << xaddr << setr_typ_rw;
    SC_METHOD(assign_testdouts_0_127);
    sensitive << xaddr << stability_time_rw;
    SC_METHOD(assign_testdouts_0_128);
    sensitive << xaddr << rx_rxhs_setr_ovr_rw << rx_rxhs_setr_ovr_en_rw << rxclk_rxhs_osc_res_en_ovr_rw << rxclk_rxhs_osc_res_en_ovr_en_rw << rxclk_rxhs_osc_pon_ovr_rw << rxclk_rxhs_osc_pon_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_129);
    sensitive << xaddr << burnin_cal_en_ovr_rw << burnin_cal_en_ovr_en_rw << norexttune_done_ovr_rw << norexttune_done_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_130);
    sensitive << xaddr << count_osc0;
    SC_METHOD(assign_testdouts_0_131);
    sensitive << xaddr << count_osc0;
    SC_METHOD(assign_testdouts_0_132);
    sensitive << xaddr << count_osc1;
    SC_METHOD(assign_testdouts_0_133);
    sensitive << xaddr << setr_burnin << count_osc1;
    SC_METHOD(assign_testdouts_0_134);
    sensitive << xaddr << noext_burn_error << norexttune_done << rx_rxhs_setr << rxclk_rxhs_osc_res_en << rxclk_rxhs_osc_pon;
    SC_METHOD(assign_testdouts_0_135);
    sensitive << xaddr << hsrx_en_bypass_clklane_rw << bintpon_clklane_rw << bintpon_bypass_clklane_rw << atb_sel_clklane_rw;
    SC_METHOD(assign_testdouts_0_136);
    sensitive << xaddr << hsrxpon_if_clklane_rw << hsrxpon_bypass_clklane_rw << hsrxenterm_if_clklane_rw << hsrxenterm_bypass_clklane_rw << hsrx_en_if_clklane_tester_rw;
    SC_METHOD(assign_testdouts_0_137);
    sensitive << xaddr << lprxponcd_bypass_clklane_rw;
    SC_METHOD(assign_testdouts_0_138);
    sensitive << xaddr << lprxponulp_clklane_rw << lprxponulp_bypass_clklane_rw << lprxponlp_clklane_rw << lprxponlp_bypass_clklane_rw << lprxponcd_clklane_rw;
    SC_METHOD(assign_testdouts_0_139);
    sensitive << xaddr << rxclk_rxhs_pull_long_channel_if_rw << rxclk_rxhs_int_clk_sel_rw << rxclk_rxhs_feed_int_clk_ovr_en_rw << rxclk_rxhs_feed_int_clk_ovr_rw << rxclk_rxhs_ddr_clk_en_if_rw << rxclk_rxhs_clk_to_long_channel_if_rw << polarity_clklane_rw;
    SC_METHOD(assign_testdouts_0_140);
    sensitive << xaddr << rxclk_rxhs_rx_en200mode_if_rw << rxclk_rxhs_ddr_clk_sel_if_rw << rxclk_rxhs_ddr_clk_div_ovr_rw << rxclk_rxhs_ddr_clk_div_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_141);
    sensitive << xaddr << rxclk_lb_prog_if_rw << rxclk_lb_res_cm_en_if_rw << rxclk_lb_res_dn_en_if_rw << rxclk_lb_res_dp_en_if_rw;
    SC_METHOD(assign_testdouts_0_142);
    sensitive << xaddr << lprxdoutlp_clklane_if << lprxdoutcd_clklane_if << hsrx_en_if_clklane << enable_clklane << bintpon_if_clklane;
    SC_METHOD(assign_testdouts_0_143);
    sensitive << xaddr << lprxdoutulp_clklane_if;
    SC_METHOD(assign_testdouts_0_144);
    sensitive << xaddr << rxclk_rxhs_rx_enterm_if << rxclk_rxhs_rx_enlogic_if << rxclk_rxhs_rx_en_if << rxclk_rxhs_osc_out_if << rxclk_rxhs_ddr_clk_if << rxclk_rxhs_ddr_clk_div << lsrxclk_clklane;
    SC_METHOD(assign_testdouts_0_145);
    sensitive << xaddr << rxclk_rxlp_ponulp_if << rxclk_rxlp_ponlp_if << rxclk_rxlp_poncd_if << rxclk_rxhs_rx_pon_if;
    SC_METHOD(assign_testdouts_0_146);
    sensitive << xaddr << rxclk_rxhs_clk_offset_cal_ovr_rw << rxclk_caldone_ovr_rw << rxclk_caldone_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_147);
    sensitive << xaddr << rxclk_start_calibration_ovr_rw << rxclk_start_calibration_ovr_en_rw << rxclk_rxhs_start_calibration_ovr_rw << rxclk_rxhs_start_calibration_ovr_en_rw << rxclk_rxhs_clk_offset_cal_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_148);
    sensitive << xaddr << rxclk_rxhs_clk_m_cal << rxclk_errcal << rxclk_caldone;
    SC_METHOD(assign_testdouts_0_149);
    sensitive << xaddr << rxclk_rxhs_start_calibration << rxclk_rxhs_clk_p_cal << rxclk_rxhs_clk_offset_cal;
    SC_METHOD(assign_testdouts_0_150);
    sensitive << xaddr << rxclk_start_calibration;
    SC_METHOD(assign_testdouts_0_151);
    sensitive << xaddr << hsrx_en_bypass_lane0_rw << bintpon_lane0_rw << bintpon_bypass_lane0_rw << atb_sel_lane0_rw << atb_lptx1200_on_lane0_rw;
    SC_METHOD(assign_testdouts_0_152);
    sensitive << xaddr << hsrxpon_if_lane0_rw << hsrxpon_bypass_lane0_rw << hsrxenterm_if_lane0_rw << hsrxenterm_bypass_lane0_rw << hsrx_en_if_lane0_tester_rw;
    SC_METHOD(assign_testdouts_0_153);
    sensitive << xaddr << lprxponcd_bypass_lane0_rw;
    SC_METHOD(assign_testdouts_0_154);
    sensitive << xaddr << lptxdin_lane0_rw << lptxdin_bypass_lane0_rw << lprxponulp_lane0_rw << lprxponulp_bypass_lane0_rw << lprxponlp_lane0_rw << lprxponlp_bypass_lane0_rw << lprxponcd_lane0_rw;
    SC_METHOD(assign_testdouts_0_155);
    sensitive << xaddr << lptxpon_lane0_rw << lptxpon_bypass_lane0_rw << lptxenulp_lane0_rw << lptxenulp_bypass_lane0_rw << lptxenpulldwn_lane0_rw << lptxenpulldwn_bypass_lane0_rw << lptxena_lane0_rw << lptxena_bypass_lane0_rw;
    SC_METHOD(assign_testdouts_0_156);
    sensitive << xaddr << rx0_rxhs_feed_int_clk_ovr_en_rw << rx0_rxhs_feed_int_clk_ovr_rw << polarity_lane0_rw;
    SC_METHOD(assign_testdouts_0_157);
    sensitive << xaddr << rx0_rxhs_clk_from_long_channel_if_rw << recal_lane0_ovr_rw << recal_lane0_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_158);
    sensitive << xaddr << rx_des_en_lane0_rw << rx_des_en_bypass_lane0_rw << rx0_txlp_sr_bypass_if_rw << rx0_txlp_eniocont_if_rw << rx0_rxhs_data_eq_if_rw;
    SC_METHOD(assign_testdouts_0_159);
    sensitive << xaddr << rx0_pm_sample_error_counter_rw << rx0_pm_start_control_ovr_en_rw << rx0_pm_start_control_ovr_rw << rx0_pm_mode_r_rw << rx0_data_retiming_out_swap_sel_rw;
    SC_METHOD(assign_testdouts_0_160);
    sensitive << xaddr << rx0_pm_man_error_latch_rw << rx0_lb_prog_if_rw << rx0_lb_res_cm_en_if_rw << rx0_lb_res_dn_en_if_rw << rx0_lb_res_dp_en_if_rw;
    SC_METHOD(assign_testdouts_0_161);
    sensitive << xaddr << rx0_rxhs_deserial_data_en_if << hsrx_en_if_lane0 << enable_lane0 << bintpon_if_lane0;
    SC_METHOD(assign_testdouts_0_162);
    sensitive << xaddr << lptxpon_if_lane0 << lptxenulp_if_lane0 << lptxenpulldn_if_lane0 << lptxena_if_lane0 << lptxdin_if_lane0;
    SC_METHOD(assign_testdouts_0_163);
    sensitive << xaddr << rx0_rxhs_rx_enterm_if << rx0_rxhs_rx_enlogic_if << rx0_rxhs_rx_en_if;
    SC_METHOD(assign_testdouts_0_164);
    sensitive << xaddr << lprxdoutlp_lane0 << lprxdoutcd_lane0 << rx0_rxhs_rx_pon_if;
    SC_METHOD(assign_testdouts_0_165);
    sensitive << xaddr << rx0_deskewcalfailed_if << rx0_deskewcaldone_if << lsrxclk_lane0 << lprxdoutulp_lane0;
    SC_METHOD(assign_testdouts_0_166);
    sensitive << xaddr << rx0_rxhs_data_if;
    SC_METHOD(assign_testdouts_0_167);
    sensitive << xaddr << rx0_pm_error_pm << rx0_txlp_iocont_if << rx0_rxhs_word_clk_if << rx0_rxhs_feed_int_clk_if << rx0_rxhs_des_en_if;
    SC_METHOD(assign_testdouts_0_168);
    sensitive << xaddr << rx0_pm_error_counter;
    SC_METHOD(assign_testdouts_0_169);
    sensitive << xaddr << rx0_pm_error_counter;
    SC_METHOD(assign_testdouts_0_170);
    sensitive << xaddr << rx0_pm_start;
    SC_METHOD(assign_testdouts_0_171);
    sensitive << xaddr << tx0_txlp_enulp << tx0_txlp_ena << tx0_txlp_enlogic << tx0_txlp_pon;
    SC_METHOD(assign_testdouts_0_172);
    sensitive << xaddr << tx0_rxlp_ponulp << tx0_rxlp_ponlp << tx0_rxlp_poncd << tx0_txlp_sr_bypass << tx0_txlp_enpulldwn << tx0_txlp_din;
    SC_METHOD(assign_testdouts_0_173);
    sensitive << xaddr << rx0_rxhs_data_offset_cal_en_ovr_en_rw << rx0_cal_done_ovr_rw << rx0_cal_done_ovr_en_rw << rx0_start_calibration_ovr_rw << rx0_start_calibration_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_174);
    sensitive << xaddr << rx0_rxhs_data_offset_cal_ovr_en_rw << rx0_rxhs_data_offset_cal_ovr_rw << rx0_rxhs_data_offset_cal_en_ovr_rw;
    SC_METHOD(assign_testdouts_0_175);
    sensitive << xaddr << rx0_rxhs_data_ovr_rw;
    SC_METHOD(assign_testdouts_0_176);
    sensitive << xaddr << rx0_rxhs_repeatcal_rw << rx0_rxhs_data_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_177);
    sensitive << xaddr << rx0_cal_error << rx0_cal_done << rx0_start_calibration;
    SC_METHOD(assign_testdouts_0_178);
    sensitive << xaddr << rx0_rxhs_data_int;
    SC_METHOD(assign_testdouts_0_179);
    sensitive << xaddr << rx0_rxhs_data_offset_cal_en << rx0_rxhs_data_offset_cal;
    SC_METHOD(assign_testdouts_0_180);
    sensitive << xaddr << rx0_total_ones;
    SC_METHOD(assign_testdouts_0_181);
    sensitive << xaddr << rx0_ddl_start_calibration_if << rx0_ddl_finished_if << rx0_ddl_error_flag_if << ddl_tune_en_lane0_if;
    SC_METHOD(assign_testdouts_0_182);
    sensitive << xaddr << rx0_freq_meas_out_if;
    SC_METHOD(assign_testdouts_0_183);
    sensitive << xaddr << rx0_freq_meas_out_if;
    SC_METHOD(assign_testdouts_0_184);
    sensitive << xaddr << rx0_osc_freq_target_if;
    SC_METHOD(assign_testdouts_0_185);
    sensitive << xaddr << rx0_osc_freq_target_if;
    SC_METHOD(assign_testdouts_0_186);
    sensitive << xaddr << rx0_rxhs_ddl_phase_change_p_if << rx0_rxhs_ddl_tune_if;
    SC_METHOD(assign_testdouts_0_187);
    sensitive << xaddr << rx0_rxhs_ddl_phase_clk_if;
    SC_METHOD(assign_testdouts_0_188);
    sensitive << xaddr << rx0_rxhs_ddl_phase_data_if;
    SC_METHOD(assign_testdouts_0_189);
    sensitive << xaddr << rx0_rxhs_ddl_tune_en_if_rw << rx0_rxhs_ddl_tune_en_bypass_rw << rx0_ddl_start_calibration_ovr_rw << rx0_ddl_start_calibration_ovr_en_rw << rx0_ddl_finished_ovr_rw << rx0_ddl_finished_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_190);
    sensitive << xaddr << rx0_rxhs_ddl_phase_change_p_ovr_rw << rx0_rxhs_ddl_phase_change_p_ovr_en_rw << rx0_rxhs_ddl_tune_ovr_en_rw << rx0_rxhs_ddl_tune_ovr_rw;
    SC_METHOD(assign_testdouts_0_191);
    sensitive << xaddr << rx0_rxhs_ddl_phase_clk_ovr_en_rw << rx0_rxhs_ddl_phase_clk_ovr_rw;
    SC_METHOD(assign_testdouts_0_192);
    sensitive << xaddr << rx0_rxhs_ddl_phase_data_ovr_en_rw << rx0_rxhs_ddl_phase_data_ovr_rw;
    SC_METHOD(assign_testdouts_0_193);
    sensitive << xaddr << rx0_osc_freq_target_if_ovr_rw;
    SC_METHOD(assign_testdouts_0_194);
    sensitive << xaddr << rx0_osc_freq_target_if_ovr_rw;
    SC_METHOD(assign_testdouts_0_195);
    sensitive << xaddr << rx0_osc_freq_target_if_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_196);
    sensitive << xaddr << hsrx_en_bypass_lane1_rw << bintpon_lane1_rw << bintpon_bypass_lane1_rw << atb_sel_lane1_rw;
    SC_METHOD(assign_testdouts_0_197);
    sensitive << xaddr << hsrxpon_if_lane1_rw << hsrxpon_bypass_lane1_rw << hsrxenterm_if_lane1_rw << hsrxenterm_bypass_lane1_rw << hsrx_en_if_lane1_tester_rw;
    SC_METHOD(assign_testdouts_0_198);
    sensitive << xaddr << lprxponcd_bypass_lane1_rw;
    SC_METHOD(assign_testdouts_0_199);
    sensitive << xaddr << lprxponulp_lane1_rw << lprxponulp_bypass_lane1_rw << lprxponlp_lane1_rw << lprxponlp_bypass_lane1_rw << lprxponcd_lane1_rw;
    SC_METHOD(assign_testdouts_0_200);
    sensitive << xaddr << rx1_rxhs_feed_int_clk_ovr_en_rw << rx1_rxhs_feed_int_clk_ovr_rw << polarity_lane1_rw;
    SC_METHOD(assign_testdouts_0_201);
    sensitive << xaddr << rx1_rxhs_clk_from_long_channel_if_rw << recal_lane1_ovr_rw << recal_lane1_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_202);
    sensitive << xaddr << rx_des_en_lane1_rw << rx_des_en_bypass_lane1_rw << rx1_rxhs_data_eq_if_rw;
    SC_METHOD(assign_testdouts_0_203);
    sensitive << xaddr << rx1_pm_sample_error_counter_rw << rx1_pm_start_control_ovr_en_rw << rx1_pm_start_control_ovr_rw << rx1_pm_mode_r_rw << rx1_data_retiming_out_swap_sel_rw;
    SC_METHOD(assign_testdouts_0_204);
    sensitive << xaddr << rx1_rxhs_ddl_phase_clk_ovr_en_rw << rx1_rxhs_ddl_phase_clk_ovr_rw << rx1_rxhs_ddl_phase_change_p_ovr_en_rw << rx1_rxhs_ddl_phase_change_p_ovr_rw;
    SC_METHOD(assign_testdouts_0_205);
    sensitive << xaddr << rx1_rxhs_ddl_phase_data_ovr_en_rw << rx1_rxhs_ddl_phase_data_ovr_rw;
    SC_METHOD(assign_testdouts_0_206);
    sensitive << xaddr << rx1_pm_man_error_latch_rw << rx1_lb_prog_if_rw << rx1_lb_res_cm_en_if_rw << rx1_lb_res_dn_en_if_rw << rx1_lb_res_dp_en_if_rw;
    SC_METHOD(assign_testdouts_0_207);
    sensitive << xaddr << rx1_rxhs_deserial_data_en_if << hsrx_en_if_lane1 << enable_lane1 << bintpon_if_lane1;
    SC_METHOD(assign_testdouts_0_208);
    sensitive << xaddr << rx1_rxhs_rx_enterm_if << rx1_rxhs_rx_enlogic_if << rx1_rxhs_rx_en_if;
    SC_METHOD(assign_testdouts_0_209);
    sensitive << xaddr << lprxdoutlp_lane1 << lprxdoutcd_lane1 << rx1_rxlp_ponulp_if << rx1_rxlp_ponlp_if << rx1_rxlp_poncd_if << rx1_rxhs_rx_pon_if;
    SC_METHOD(assign_testdouts_0_210);
    sensitive << xaddr << rx1_deskewcalfailed_if << rx1_deskewcaldone_if << lsrxclk_lane1 << lprxdoutulp_lane1;
    SC_METHOD(assign_testdouts_0_211);
    sensitive << xaddr << rx1_rxhs_data_if;
    SC_METHOD(assign_testdouts_0_212);
    sensitive << xaddr << rx1_pm_error_pm << rx1_rxhs_word_clk_if << rx1_rxhs_feed_int_clk_if << rx1_rxhs_des_en_if;
    SC_METHOD(assign_testdouts_0_213);
    sensitive << xaddr << rx1_pm_error_counter;
    SC_METHOD(assign_testdouts_0_214);
    sensitive << xaddr << rx1_pm_error_counter;
    SC_METHOD(assign_testdouts_0_215);
    sensitive << xaddr << rx1_rxhs_ddl_phase_clk_if << rx1_rxhs_ddl_phase_change_p_if << rx1_pm_start;
    SC_METHOD(assign_testdouts_0_216);
    sensitive << xaddr << rx1_rxhs_ddl_phase_data_if;
    SC_METHOD(assign_testdouts_0_217);
    sensitive << xaddr << rx1_rxhs_data_offset_cal_en_ovr_en_rw << rx1_cal_done_ovr_rw << rx1_cal_done_ovr_en_rw << rx1_start_calibration_ovr_rw << rx1_start_calibration_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_218);
    sensitive << xaddr << rx1_rxhs_data_offset_cal_ovr_en_rw << rx1_rxhs_data_offset_cal_ovr_rw << rx1_rxhs_data_offset_cal_en_ovr_rw;
    SC_METHOD(assign_testdouts_0_219);
    sensitive << xaddr << rx1_rxhs_data_ovr_rw;
    SC_METHOD(assign_testdouts_0_220);
    sensitive << xaddr << rx1_rxhs_repeatcal_rw << rx1_rxhs_data_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_221);
    sensitive << xaddr << rx1_cal_error << rx1_cal_done << rx1_start_calibration;
    SC_METHOD(assign_testdouts_0_222);
    sensitive << xaddr << rx1_rxhs_data_int;
    SC_METHOD(assign_testdouts_0_223);
    sensitive << xaddr << rx1_rxhs_data_offset_cal_en << rx1_rxhs_data_offset_cal;
    SC_METHOD(assign_testdouts_0_224);
    sensitive << xaddr << rx1_total_ones;
    SC_METHOD(assign_testdouts_0_225);
    sensitive << xaddr << rx1_ddl_start_calibration_if << rx1_ddl_finished_if << rx1_ddl_error_flag_if << ddl_tune_en_lane1_if;
    SC_METHOD(assign_testdouts_0_226);
    sensitive << xaddr << rx1_freq_meas_out_if;
    SC_METHOD(assign_testdouts_0_227);
    sensitive << xaddr << rx1_freq_meas_out_if;
    SC_METHOD(assign_testdouts_0_228);
    sensitive << xaddr << rx1_osc_freq_target_if;
    SC_METHOD(assign_testdouts_0_229);
    sensitive << xaddr << rx1_osc_freq_target_if;
    SC_METHOD(assign_testdouts_0_230);
    sensitive << xaddr << rx1_rxhs_ddl_tune_if;
    SC_METHOD(assign_testdouts_0_231);
    sensitive << xaddr << rx1_rxhs_ddl_tune_en_if_rw << rx1_rxhs_ddl_tune_en_bypass_rw << rx1_ddl_start_calibration_ovr_rw << rx1_ddl_start_calibration_ovr_en_rw << rx1_ddl_finished_ovr_rw << rx1_ddl_finished_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_232);
    sensitive << xaddr << rx1_rxhs_ddl_tune_ovr_en_rw << rx1_rxhs_ddl_tune_ovr_rw;
    SC_METHOD(assign_testdouts_0_233);
    sensitive << xaddr << rx1_osc_freq_target_if_ovr_rw;
    SC_METHOD(assign_testdouts_0_234);
    sensitive << xaddr << rx1_osc_freq_target_if_ovr_rw;
    SC_METHOD(assign_testdouts_0_235);
    sensitive << xaddr << rx1_osc_freq_target_if_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_236);
    sensitive << xaddr << hsrx_en_bypass_lane2_rw << bintpon_lane2_rw << bintpon_bypass_lane2_rw << atb_sel_lane2_rw;
    SC_METHOD(assign_testdouts_0_237);
    sensitive << xaddr << hsrxpon_if_lane2_rw << hsrxpon_bypass_lane2_rw << hsrxenterm_if_lane2_rw << hsrxenterm_bypass_lane2_rw << hsrx_en_if_lane2_tester_rw;
    SC_METHOD(assign_testdouts_0_238);
    sensitive << xaddr << lprxponcd_bypass_lane2_rw;
    SC_METHOD(assign_testdouts_0_239);
    sensitive << xaddr << lprxponulp_lane2_rw << lprxponulp_bypass_lane2_rw << lprxponlp_lane2_rw << lprxponlp_bypass_lane2_rw << lprxponcd_lane2_rw;
    SC_METHOD(assign_testdouts_0_240);
    sensitive << xaddr << rx2_rxhs_feed_int_clk_ovr_en_rw << rx2_rxhs_feed_int_clk_ovr_rw << polarity_lane2_rw;
    SC_METHOD(assign_testdouts_0_241);
    sensitive << xaddr << rx2_rxhs_clk_from_long_channel_if_rw << recal_lane2_ovr_rw << recal_lane2_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_242);
    sensitive << xaddr << rx_des_en_lane2_rw << rx_des_en_bypass_lane2_rw << rx2_rxhs_data_eq_if_rw;
    SC_METHOD(assign_testdouts_0_243);
    sensitive << xaddr << rx2_pm_sample_error_counter_rw << rx2_pm_start_control_ovr_en_rw << rx2_pm_start_control_ovr_rw << rx2_pm_mode_r_rw << rx2_data_retiming_out_swap_sel_rw;
    SC_METHOD(assign_testdouts_0_244);
    sensitive << xaddr << rx2_rxhs_ddl_phase_clk_ovr_en_rw << rx2_rxhs_ddl_phase_clk_ovr_rw << rx2_rxhs_ddl_phase_change_p_ovr_en_rw << rx2_rxhs_ddl_phase_change_p_ovr_rw;
    SC_METHOD(assign_testdouts_0_245);
    sensitive << xaddr << rx2_rxhs_ddl_phase_data_ovr_en_rw << rx2_rxhs_ddl_phase_data_ovr_rw;
    SC_METHOD(assign_testdouts_0_246);
    sensitive << xaddr << rx2_pm_man_error_latch_rw << rx2_lb_prog_if_rw << rx2_lb_res_cm_en_if_rw << rx2_lb_res_dn_en_if_rw << rx2_lb_res_dp_en_if_rw;
    SC_METHOD(assign_testdouts_0_247);
    sensitive << xaddr << rx2_rxhs_deserial_data_en_if << hsrx_en_if_lane2 << enable_lane2 << bintpon_if_lane2;
    SC_METHOD(assign_testdouts_0_248);
    sensitive << xaddr << rx2_rxhs_rx_enterm_if << rx2_rxhs_rx_enlogic_if << rx2_rxhs_rx_en_if;
    SC_METHOD(assign_testdouts_0_249);
    sensitive << xaddr << lprxdoutlp_lane2 << lprxdoutcd_lane2 << rx2_rxlp_ponulp_if << rx2_rxlp_ponlp_if << rx2_rxlp_poncd_if << rx2_rxhs_rx_pon_if;
    SC_METHOD(assign_testdouts_0_250);
    sensitive << xaddr << rx2_deskewcalfailed_if << rx2_deskewcaldone_if << lsrxclk_lane2 << lprxdoutulp_lane2;
    SC_METHOD(assign_testdouts_0_251);
    sensitive << xaddr << rx2_rxhs_data_if;
    SC_METHOD(assign_testdouts_0_252);
    sensitive << xaddr << rx2_pm_error_pm << rx2_rxhs_word_clk_if << rx2_rxhs_feed_int_clk_if << rx2_rxhs_des_en_if;
    SC_METHOD(assign_testdouts_0_253);
    sensitive << xaddr << rx2_pm_error_counter;
    SC_METHOD(assign_testdouts_0_254);
    sensitive << xaddr << rx2_pm_error_counter;
    SC_METHOD(assign_testdouts_0_255);
    sensitive << xaddr << rx2_rxhs_ddl_phase_clk_if << rx2_rxhs_ddl_phase_change_p_if << rx2_pm_start;
    SC_METHOD(assign_testdouts_0_256);
    sensitive << xaddr << rx2_rxhs_ddl_phase_data_if;
    SC_METHOD(assign_testdouts_0_257);
    sensitive << xaddr << rx2_rxhs_data_offset_cal_en_ovr_en_rw << rx2_cal_done_ovr_rw << rx2_cal_done_ovr_en_rw << rx2_start_calibration_ovr_rw << rx2_start_calibration_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_258);
    sensitive << xaddr << rx2_rxhs_data_offset_cal_ovr_en_rw << rx2_rxhs_data_offset_cal_ovr_rw << rx2_rxhs_data_offset_cal_en_ovr_rw;
    SC_METHOD(assign_testdouts_0_259);
    sensitive << xaddr << rx2_rxhs_data_ovr_rw;
    SC_METHOD(assign_testdouts_0_260);
    sensitive << xaddr << rx2_rxhs_repeatcal_rw << rx2_rxhs_data_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_261);
    sensitive << xaddr << rx2_cal_error << rx2_cal_done << rx2_start_calibration;
    SC_METHOD(assign_testdouts_0_262);
    sensitive << xaddr << rx2_rxhs_data_int;
    SC_METHOD(assign_testdouts_0_263);
    sensitive << xaddr << rx2_rxhs_data_offset_cal_en << rx2_rxhs_data_offset_cal;
    SC_METHOD(assign_testdouts_0_264);
    sensitive << xaddr << rx2_total_ones;
    SC_METHOD(assign_testdouts_0_265);
    sensitive << xaddr << rx2_ddl_start_calibration_if << rx2_ddl_finished_if << rx2_ddl_error_flag_if << ddl_tune_en_lane2_if;
    SC_METHOD(assign_testdouts_0_266);
    sensitive << xaddr << rx2_freq_meas_out_if;
    SC_METHOD(assign_testdouts_0_267);
    sensitive << xaddr << rx2_freq_meas_out_if;
    SC_METHOD(assign_testdouts_0_268);
    sensitive << xaddr << rx2_osc_freq_target_if;
    SC_METHOD(assign_testdouts_0_269);
    sensitive << xaddr << rx2_osc_freq_target_if;
    SC_METHOD(assign_testdouts_0_270);
    sensitive << xaddr << rx2_rxhs_ddl_tune_if;
    SC_METHOD(assign_testdouts_0_271);
    sensitive << xaddr << rx2_rxhs_ddl_tune_en_if_rw << rx2_rxhs_ddl_tune_en_bypass_rw << rx2_ddl_start_calibration_ovr_rw << rx2_ddl_start_calibration_ovr_en_rw << rx2_ddl_finished_ovr_rw << rx2_ddl_finished_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_272);
    sensitive << xaddr << rx2_rxhs_ddl_tune_ovr_en_rw << rx2_rxhs_ddl_tune_ovr_rw;
    SC_METHOD(assign_testdouts_0_273);
    sensitive << xaddr << rx2_osc_freq_target_if_ovr_rw;
    SC_METHOD(assign_testdouts_0_274);
    sensitive << xaddr << rx2_osc_freq_target_if_ovr_rw;
    SC_METHOD(assign_testdouts_0_275);
    sensitive << xaddr << rx2_osc_freq_target_if_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_276);
    sensitive << xaddr << hsrx_en_bypass_lane3_rw << bintpon_lane3_rw << bintpon_bypass_lane3_rw << atb_sel_lane3_rw;
    SC_METHOD(assign_testdouts_0_277);
    sensitive << xaddr << hsrxpon_if_lane3_rw << hsrxpon_bypass_lane3_rw << hsrxenterm_if_lane3_rw << hsrxenterm_bypass_lane3_rw << hsrx_en_if_lane3_tester_rw;
    SC_METHOD(assign_testdouts_0_278);
    sensitive << xaddr << lprxponcd_bypass_lane3_rw;
    SC_METHOD(assign_testdouts_0_279);
    sensitive << xaddr << lprxponulp_lane3_rw << lprxponulp_bypass_lane3_rw << lprxponlp_lane3_rw << lprxponlp_bypass_lane3_rw << lprxponcd_lane3_rw;
    SC_METHOD(assign_testdouts_0_280);
    sensitive << xaddr << rx3_rxhs_feed_int_clk_ovr_en_rw << rx3_rxhs_feed_int_clk_ovr_rw << polarity_lane3_rw;
    SC_METHOD(assign_testdouts_0_281);
    sensitive << xaddr << rx3_rxhs_clk_from_long_channel_if_rw << recal_lane3_ovr_rw << recal_lane3_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_282);
    sensitive << xaddr << rx_des_en_lane3_rw << rx_des_en_bypass_lane3_rw << rx3_rxhs_data_eq_if_rw;
    SC_METHOD(assign_testdouts_0_283);
    sensitive << xaddr << rx3_pm_sample_error_counter_rw << rx3_pm_start_control_ovr_en_rw << rx3_pm_start_control_ovr_rw << rx3_pm_mode_r_rw << rx3_data_retiming_out_swap_sel_rw;
    SC_METHOD(assign_testdouts_0_284);
    sensitive << xaddr << rx3_rxhs_ddl_phase_clk_ovr_en_rw << rx3_rxhs_ddl_phase_clk_ovr_rw << rx3_rxhs_ddl_phase_change_p_ovr_en_rw << rx3_rxhs_ddl_phase_change_p_ovr_rw;
    SC_METHOD(assign_testdouts_0_285);
    sensitive << xaddr << rx3_rxhs_ddl_phase_data_ovr_en_rw << rx3_rxhs_ddl_phase_data_ovr_rw;
    SC_METHOD(assign_testdouts_0_286);
    sensitive << xaddr << rx3_pm_man_error_latch_rw << rx3_lb_prog_if_rw << rx3_lb_res_cm_en_if_rw << rx3_lb_res_dn_en_if_rw << rx3_lb_res_dp_en_if_rw;
    SC_METHOD(assign_testdouts_0_287);
    sensitive << xaddr << rx3_rxhs_deserial_data_en_if << hsrx_en_if_lane3 << enable_lane3 << bintpon_if_lane3;
    SC_METHOD(assign_testdouts_0_288);
    sensitive << xaddr << rx3_rxhs_rx_enterm_if << rx3_rxhs_rx_enlogic_if << rx3_rxhs_rx_en_if;
    SC_METHOD(assign_testdouts_0_289);
    sensitive << xaddr << lprxdoutlp_lane3 << lprxdoutcd_lane3 << rx3_rxlp_ponulp_if << rx3_rxlp_ponlp_if << rx3_rxlp_poncd_if << rx3_rxhs_rx_pon_if;
    SC_METHOD(assign_testdouts_0_290);
    sensitive << xaddr << rx3_deskewcalfailed_if << rx3_deskewcaldone_if << lsrxclk_lane3 << lprxdoutulp_lane3;
    SC_METHOD(assign_testdouts_0_291);
    sensitive << xaddr << rx3_rxhs_data_if;
    SC_METHOD(assign_testdouts_0_292);
    sensitive << xaddr << rx3_pm_error_pm << rx3_rxhs_word_clk_if << rx3_rxhs_feed_int_clk_if << rx3_rxhs_des_en_if;
    SC_METHOD(assign_testdouts_0_293);
    sensitive << xaddr << rx3_pm_error_counter;
    SC_METHOD(assign_testdouts_0_294);
    sensitive << xaddr << rx3_pm_error_counter;
    SC_METHOD(assign_testdouts_0_295);
    sensitive << xaddr << rx3_rxhs_ddl_phase_clk_if << rx3_rxhs_ddl_phase_change_p_if << rx3_pm_start;
    SC_METHOD(assign_testdouts_0_296);
    sensitive << xaddr << rx3_rxhs_ddl_phase_data_if;
    SC_METHOD(assign_testdouts_0_297);
    sensitive << xaddr << rx3_rxhs_data_offset_cal_en_ovr_en_rw << rx3_cal_done_ovr_rw << rx3_cal_done_ovr_en_rw << rx3_start_calibration_ovr_rw << rx3_start_calibration_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_298);
    sensitive << xaddr << rx3_rxhs_data_offset_cal_ovr_en_rw << rx3_rxhs_data_offset_cal_ovr_rw << rx3_rxhs_data_offset_cal_en_ovr_rw;
    SC_METHOD(assign_testdouts_0_299);
    sensitive << xaddr << rx3_rxhs_data_ovr_rw;
    SC_METHOD(assign_testdouts_0_300);
    sensitive << xaddr << rx3_rxhs_repeatcal_rw << rx3_rxhs_data_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_301);
    sensitive << xaddr << rx3_cal_error << rx3_cal_done << rx3_start_calibration;
    SC_METHOD(assign_testdouts_0_302);
    sensitive << xaddr << rx3_rxhs_data_int;
    SC_METHOD(assign_testdouts_0_303);
    sensitive << xaddr << rx3_rxhs_data_offset_cal_en << rx3_rxhs_data_offset_cal;
    SC_METHOD(assign_testdouts_0_304);
    sensitive << xaddr << rx3_total_ones;
    SC_METHOD(assign_testdouts_0_305);
    sensitive << xaddr << rx3_ddl_start_calibration_if << rx3_ddl_finished_if << rx3_ddl_error_flag_if << ddl_tune_en_lane3_if;
    SC_METHOD(assign_testdouts_0_306);
    sensitive << xaddr << rx3_freq_meas_out_if;
    SC_METHOD(assign_testdouts_0_307);
    sensitive << xaddr << rx3_freq_meas_out_if;
    SC_METHOD(assign_testdouts_0_308);
    sensitive << xaddr << rx3_osc_freq_target_if;
    SC_METHOD(assign_testdouts_0_309);
    sensitive << xaddr << rx3_osc_freq_target_if;
    SC_METHOD(assign_testdouts_0_310);
    sensitive << xaddr << rx3_rxhs_ddl_tune_if;
    SC_METHOD(assign_testdouts_0_311);
    sensitive << xaddr << rx3_rxhs_ddl_tune_en_if_rw << rx3_rxhs_ddl_tune_en_bypass_rw << rx3_ddl_start_calibration_ovr_rw << rx3_ddl_start_calibration_ovr_en_rw << rx3_ddl_finished_ovr_rw << rx3_ddl_finished_ovr_en_rw;
    SC_METHOD(assign_testdouts_0_312);
    sensitive << xaddr << rx3_rxhs_ddl_tune_ovr_en_rw << rx3_rxhs_ddl_tune_ovr_rw;
    SC_METHOD(assign_testdouts_0_313);
    sensitive << xaddr << rx3_osc_freq_target_if_ovr_rw;
    SC_METHOD(assign_testdouts_0_314);
    sensitive << xaddr << rx3_osc_freq_target_if_ovr_rw;
    SC_METHOD(assign_testdouts_0_315);
    sensitive << xaddr << rx3_osc_freq_target_if_ovr_en_rw;
    SC_METHOD(proc_always_823);
    sensitive << testclkinv.pos() << testclr.neg();
    dont_initialize();
    SC_METHOD(proc_always_839);
    sensitive << testclk.pos() << testclr.neg();
    dont_initialize();
    SC_METHOD(proc_always_1354);
    sensitive << testclk.pos() << testclr.neg();
    dont_initialize();
}

void mipi_4_rx_dphy_ns_dphy4rxtester::merge_testdout() {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd(testdout_b[0]));
    v.set_bit(1, vl::rd(testdout_b[1]));
    v.set_bit(2, vl::rd(testdout_b[2]));
    v.set_bit(3, vl::rd(testdout_b[3]));
    v.set_bit(4, vl::rd(testdout_b[4]));
    v.set_bit(5, vl::rd(testdout_b[5]));
    v.set_bit(6, vl::rd(testdout_b[6]));
    v.set_bit(7, vl::rd(testdout_b[7]));
    vl::wr(testdout, v);
}

vl::logic<8> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdout() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd(testdout_b[0]));
    v.set_bit(1, vl::rd(testdout_b[1]));
    v.set_bit(2, vl::rd(testdout_b[2]));
    v.set_bit(3, vl::rd(testdout_b[3]));
    v.set_bit(4, vl::rd(testdout_b[4]));
    v.set_bit(5, vl::rd(testdout_b[5]));
    v.set_bit(6, vl::rd(testdout_b[6]));
    v.set_bit(7, vl::rd(testdout_b[7]));
    return v;
}

vl::logic<315> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdouts_0() const {
    vl::logic<315> v(0);
    v.set_bit(0, vl::rd(testdouts_0[0]));
    v.set_bit(1, vl::rd(testdouts_0[1]));
    v.set_bit(2, vl::rd(testdouts_0[2]));
    v.set_bit(3, vl::rd(testdouts_0[3]));
    v.set_bit(4, vl::rd(testdouts_0[4]));
    v.set_bit(5, vl::rd(testdouts_0[5]));
    v.set_bit(6, vl::rd(testdouts_0[6]));
    v.set_bit(7, vl::rd(testdouts_0[7]));
    v.set_bit(8, vl::rd(testdouts_0[8]));
    v.set_bit(9, vl::rd(testdouts_0[9]));
    v.set_bit(10, vl::rd(testdouts_0[10]));
    v.set_bit(11, vl::rd(testdouts_0[11]));
    v.set_bit(12, vl::rd(testdouts_0[12]));
    v.set_bit(13, vl::rd(testdouts_0[13]));
    v.set_bit(14, vl::rd(testdouts_0[14]));
    v.set_bit(15, vl::rd(testdouts_0[15]));
    v.set_bit(16, vl::rd(testdouts_0[16]));
    v.set_bit(17, vl::rd(testdouts_0[17]));
    v.set_bit(18, vl::rd(testdouts_0[18]));
    v.set_bit(19, vl::rd(testdouts_0[19]));
    v.set_bit(20, vl::rd(testdouts_0[20]));
    v.set_bit(21, vl::rd(testdouts_0[21]));
    v.set_bit(22, vl::rd(testdouts_0[22]));
    v.set_bit(23, vl::rd(testdouts_0[23]));
    v.set_bit(24, vl::rd(testdouts_0[24]));
    v.set_bit(25, vl::rd(testdouts_0[25]));
    v.set_bit(26, vl::rd(testdouts_0[26]));
    v.set_bit(27, vl::rd(testdouts_0[27]));
    v.set_bit(28, vl::rd(testdouts_0[28]));
    v.set_bit(29, vl::rd(testdouts_0[29]));
    v.set_bit(30, vl::rd(testdouts_0[30]));
    v.set_bit(31, vl::rd(testdouts_0[31]));
    v.set_bit(32, vl::rd(testdouts_0[32]));
    v.set_bit(33, vl::rd(testdouts_0[33]));
    v.set_bit(34, vl::rd(testdouts_0[34]));
    v.set_bit(35, vl::rd(testdouts_0[35]));
    v.set_bit(36, vl::rd(testdouts_0[36]));
    v.set_bit(37, vl::rd(testdouts_0[37]));
    v.set_bit(38, vl::rd(testdouts_0[38]));
    v.set_bit(39, vl::rd(testdouts_0[39]));
    v.set_bit(40, vl::rd(testdouts_0[40]));
    v.set_bit(41, vl::rd(testdouts_0[41]));
    v.set_bit(42, vl::rd(testdouts_0[42]));
    v.set_bit(43, vl::rd(testdouts_0[43]));
    v.set_bit(44, vl::rd(testdouts_0[44]));
    v.set_bit(45, vl::rd(testdouts_0[45]));
    v.set_bit(46, vl::rd(testdouts_0[46]));
    v.set_bit(47, vl::rd(testdouts_0[47]));
    v.set_bit(48, vl::rd(testdouts_0[48]));
    v.set_bit(49, vl::rd(testdouts_0[49]));
    v.set_bit(50, vl::rd(testdouts_0[50]));
    v.set_bit(51, vl::rd(testdouts_0[51]));
    v.set_bit(52, vl::rd(testdouts_0[52]));
    v.set_bit(53, vl::rd(testdouts_0[53]));
    v.set_bit(54, vl::rd(testdouts_0[54]));
    v.set_bit(55, vl::rd(testdouts_0[55]));
    v.set_bit(56, vl::rd(testdouts_0[56]));
    v.set_bit(57, vl::rd(testdouts_0[57]));
    v.set_bit(58, vl::rd(testdouts_0[58]));
    v.set_bit(59, vl::rd(testdouts_0[59]));
    v.set_bit(60, vl::rd(testdouts_0[60]));
    v.set_bit(61, vl::rd(testdouts_0[61]));
    v.set_bit(62, vl::rd(testdouts_0[62]));
    v.set_bit(63, vl::rd(testdouts_0[63]));
    v.set_bit(64, vl::rd(testdouts_0[64]));
    v.set_bit(65, vl::rd(testdouts_0[65]));
    v.set_bit(66, vl::rd(testdouts_0[66]));
    v.set_bit(67, vl::rd(testdouts_0[67]));
    v.set_bit(68, vl::rd(testdouts_0[68]));
    v.set_bit(69, vl::rd(testdouts_0[69]));
    v.set_bit(70, vl::rd(testdouts_0[70]));
    v.set_bit(71, vl::rd(testdouts_0[71]));
    v.set_bit(72, vl::rd(testdouts_0[72]));
    v.set_bit(73, vl::rd(testdouts_0[73]));
    v.set_bit(74, vl::rd(testdouts_0[74]));
    v.set_bit(75, vl::rd(testdouts_0[75]));
    v.set_bit(76, vl::rd(testdouts_0[76]));
    v.set_bit(77, vl::rd(testdouts_0[77]));
    v.set_bit(78, vl::rd(testdouts_0[78]));
    v.set_bit(79, vl::rd(testdouts_0[79]));
    v.set_bit(80, vl::rd(testdouts_0[80]));
    v.set_bit(81, vl::rd(testdouts_0[81]));
    v.set_bit(82, vl::rd(testdouts_0[82]));
    v.set_bit(83, vl::rd(testdouts_0[83]));
    v.set_bit(84, vl::rd(testdouts_0[84]));
    v.set_bit(85, vl::rd(testdouts_0[85]));
    v.set_bit(86, vl::rd(testdouts_0[86]));
    v.set_bit(87, vl::rd(testdouts_0[87]));
    v.set_bit(88, vl::rd(testdouts_0[88]));
    v.set_bit(89, vl::rd(testdouts_0[89]));
    v.set_bit(90, vl::rd(testdouts_0[90]));
    v.set_bit(91, vl::rd(testdouts_0[91]));
    v.set_bit(92, vl::rd(testdouts_0[92]));
    v.set_bit(93, vl::rd(testdouts_0[93]));
    v.set_bit(94, vl::rd(testdouts_0[94]));
    v.set_bit(95, vl::rd(testdouts_0[95]));
    v.set_bit(96, vl::rd(testdouts_0[96]));
    v.set_bit(97, vl::rd(testdouts_0[97]));
    v.set_bit(98, vl::rd(testdouts_0[98]));
    v.set_bit(99, vl::rd(testdouts_0[99]));
    v.set_bit(100, vl::rd(testdouts_0[100]));
    v.set_bit(101, vl::rd(testdouts_0[101]));
    v.set_bit(102, vl::rd(testdouts_0[102]));
    v.set_bit(103, vl::rd(testdouts_0[103]));
    v.set_bit(104, vl::rd(testdouts_0[104]));
    v.set_bit(105, vl::rd(testdouts_0[105]));
    v.set_bit(106, vl::rd(testdouts_0[106]));
    v.set_bit(107, vl::rd(testdouts_0[107]));
    v.set_bit(108, vl::rd(testdouts_0[108]));
    v.set_bit(109, vl::rd(testdouts_0[109]));
    v.set_bit(110, vl::rd(testdouts_0[110]));
    v.set_bit(111, vl::rd(testdouts_0[111]));
    v.set_bit(112, vl::rd(testdouts_0[112]));
    v.set_bit(113, vl::rd(testdouts_0[113]));
    v.set_bit(114, vl::rd(testdouts_0[114]));
    v.set_bit(115, vl::rd(testdouts_0[115]));
    v.set_bit(116, vl::rd(testdouts_0[116]));
    v.set_bit(117, vl::rd(testdouts_0[117]));
    v.set_bit(118, vl::rd(testdouts_0[118]));
    v.set_bit(119, vl::rd(testdouts_0[119]));
    v.set_bit(120, vl::rd(testdouts_0[120]));
    v.set_bit(121, vl::rd(testdouts_0[121]));
    v.set_bit(122, vl::rd(testdouts_0[122]));
    v.set_bit(123, vl::rd(testdouts_0[123]));
    v.set_bit(124, vl::rd(testdouts_0[124]));
    v.set_bit(125, vl::rd(testdouts_0[125]));
    v.set_bit(126, vl::rd(testdouts_0[126]));
    v.set_bit(127, vl::rd(testdouts_0[127]));
    v.set_bit(128, vl::rd(testdouts_0[128]));
    v.set_bit(129, vl::rd(testdouts_0[129]));
    v.set_bit(130, vl::rd(testdouts_0[130]));
    v.set_bit(131, vl::rd(testdouts_0[131]));
    v.set_bit(132, vl::rd(testdouts_0[132]));
    v.set_bit(133, vl::rd(testdouts_0[133]));
    v.set_bit(134, vl::rd(testdouts_0[134]));
    v.set_bit(135, vl::rd(testdouts_0[135]));
    v.set_bit(136, vl::rd(testdouts_0[136]));
    v.set_bit(137, vl::rd(testdouts_0[137]));
    v.set_bit(138, vl::rd(testdouts_0[138]));
    v.set_bit(139, vl::rd(testdouts_0[139]));
    v.set_bit(140, vl::rd(testdouts_0[140]));
    v.set_bit(141, vl::rd(testdouts_0[141]));
    v.set_bit(142, vl::rd(testdouts_0[142]));
    v.set_bit(143, vl::rd(testdouts_0[143]));
    v.set_bit(144, vl::rd(testdouts_0[144]));
    v.set_bit(145, vl::rd(testdouts_0[145]));
    v.set_bit(146, vl::rd(testdouts_0[146]));
    v.set_bit(147, vl::rd(testdouts_0[147]));
    v.set_bit(148, vl::rd(testdouts_0[148]));
    v.set_bit(149, vl::rd(testdouts_0[149]));
    v.set_bit(150, vl::rd(testdouts_0[150]));
    v.set_bit(151, vl::rd(testdouts_0[151]));
    v.set_bit(152, vl::rd(testdouts_0[152]));
    v.set_bit(153, vl::rd(testdouts_0[153]));
    v.set_bit(154, vl::rd(testdouts_0[154]));
    v.set_bit(155, vl::rd(testdouts_0[155]));
    v.set_bit(156, vl::rd(testdouts_0[156]));
    v.set_bit(157, vl::rd(testdouts_0[157]));
    v.set_bit(158, vl::rd(testdouts_0[158]));
    v.set_bit(159, vl::rd(testdouts_0[159]));
    v.set_bit(160, vl::rd(testdouts_0[160]));
    v.set_bit(161, vl::rd(testdouts_0[161]));
    v.set_bit(162, vl::rd(testdouts_0[162]));
    v.set_bit(163, vl::rd(testdouts_0[163]));
    v.set_bit(164, vl::rd(testdouts_0[164]));
    v.set_bit(165, vl::rd(testdouts_0[165]));
    v.set_bit(166, vl::rd(testdouts_0[166]));
    v.set_bit(167, vl::rd(testdouts_0[167]));
    v.set_bit(168, vl::rd(testdouts_0[168]));
    v.set_bit(169, vl::rd(testdouts_0[169]));
    v.set_bit(170, vl::rd(testdouts_0[170]));
    v.set_bit(171, vl::rd(testdouts_0[171]));
    v.set_bit(172, vl::rd(testdouts_0[172]));
    v.set_bit(173, vl::rd(testdouts_0[173]));
    v.set_bit(174, vl::rd(testdouts_0[174]));
    v.set_bit(175, vl::rd(testdouts_0[175]));
    v.set_bit(176, vl::rd(testdouts_0[176]));
    v.set_bit(177, vl::rd(testdouts_0[177]));
    v.set_bit(178, vl::rd(testdouts_0[178]));
    v.set_bit(179, vl::rd(testdouts_0[179]));
    v.set_bit(180, vl::rd(testdouts_0[180]));
    v.set_bit(181, vl::rd(testdouts_0[181]));
    v.set_bit(182, vl::rd(testdouts_0[182]));
    v.set_bit(183, vl::rd(testdouts_0[183]));
    v.set_bit(184, vl::rd(testdouts_0[184]));
    v.set_bit(185, vl::rd(testdouts_0[185]));
    v.set_bit(186, vl::rd(testdouts_0[186]));
    v.set_bit(187, vl::rd(testdouts_0[187]));
    v.set_bit(188, vl::rd(testdouts_0[188]));
    v.set_bit(189, vl::rd(testdouts_0[189]));
    v.set_bit(190, vl::rd(testdouts_0[190]));
    v.set_bit(191, vl::rd(testdouts_0[191]));
    v.set_bit(192, vl::rd(testdouts_0[192]));
    v.set_bit(193, vl::rd(testdouts_0[193]));
    v.set_bit(194, vl::rd(testdouts_0[194]));
    v.set_bit(195, vl::rd(testdouts_0[195]));
    v.set_bit(196, vl::rd(testdouts_0[196]));
    v.set_bit(197, vl::rd(testdouts_0[197]));
    v.set_bit(198, vl::rd(testdouts_0[198]));
    v.set_bit(199, vl::rd(testdouts_0[199]));
    v.set_bit(200, vl::rd(testdouts_0[200]));
    v.set_bit(201, vl::rd(testdouts_0[201]));
    v.set_bit(202, vl::rd(testdouts_0[202]));
    v.set_bit(203, vl::rd(testdouts_0[203]));
    v.set_bit(204, vl::rd(testdouts_0[204]));
    v.set_bit(205, vl::rd(testdouts_0[205]));
    v.set_bit(206, vl::rd(testdouts_0[206]));
    v.set_bit(207, vl::rd(testdouts_0[207]));
    v.set_bit(208, vl::rd(testdouts_0[208]));
    v.set_bit(209, vl::rd(testdouts_0[209]));
    v.set_bit(210, vl::rd(testdouts_0[210]));
    v.set_bit(211, vl::rd(testdouts_0[211]));
    v.set_bit(212, vl::rd(testdouts_0[212]));
    v.set_bit(213, vl::rd(testdouts_0[213]));
    v.set_bit(214, vl::rd(testdouts_0[214]));
    v.set_bit(215, vl::rd(testdouts_0[215]));
    v.set_bit(216, vl::rd(testdouts_0[216]));
    v.set_bit(217, vl::rd(testdouts_0[217]));
    v.set_bit(218, vl::rd(testdouts_0[218]));
    v.set_bit(219, vl::rd(testdouts_0[219]));
    v.set_bit(220, vl::rd(testdouts_0[220]));
    v.set_bit(221, vl::rd(testdouts_0[221]));
    v.set_bit(222, vl::rd(testdouts_0[222]));
    v.set_bit(223, vl::rd(testdouts_0[223]));
    v.set_bit(224, vl::rd(testdouts_0[224]));
    v.set_bit(225, vl::rd(testdouts_0[225]));
    v.set_bit(226, vl::rd(testdouts_0[226]));
    v.set_bit(227, vl::rd(testdouts_0[227]));
    v.set_bit(228, vl::rd(testdouts_0[228]));
    v.set_bit(229, vl::rd(testdouts_0[229]));
    v.set_bit(230, vl::rd(testdouts_0[230]));
    v.set_bit(231, vl::rd(testdouts_0[231]));
    v.set_bit(232, vl::rd(testdouts_0[232]));
    v.set_bit(233, vl::rd(testdouts_0[233]));
    v.set_bit(234, vl::rd(testdouts_0[234]));
    v.set_bit(235, vl::rd(testdouts_0[235]));
    v.set_bit(236, vl::rd(testdouts_0[236]));
    v.set_bit(237, vl::rd(testdouts_0[237]));
    v.set_bit(238, vl::rd(testdouts_0[238]));
    v.set_bit(239, vl::rd(testdouts_0[239]));
    v.set_bit(240, vl::rd(testdouts_0[240]));
    v.set_bit(241, vl::rd(testdouts_0[241]));
    v.set_bit(242, vl::rd(testdouts_0[242]));
    v.set_bit(243, vl::rd(testdouts_0[243]));
    v.set_bit(244, vl::rd(testdouts_0[244]));
    v.set_bit(245, vl::rd(testdouts_0[245]));
    v.set_bit(246, vl::rd(testdouts_0[246]));
    v.set_bit(247, vl::rd(testdouts_0[247]));
    v.set_bit(248, vl::rd(testdouts_0[248]));
    v.set_bit(249, vl::rd(testdouts_0[249]));
    v.set_bit(250, vl::rd(testdouts_0[250]));
    v.set_bit(251, vl::rd(testdouts_0[251]));
    v.set_bit(252, vl::rd(testdouts_0[252]));
    v.set_bit(253, vl::rd(testdouts_0[253]));
    v.set_bit(254, vl::rd(testdouts_0[254]));
    v.set_bit(255, vl::rd(testdouts_0[255]));
    v.set_bit(256, vl::rd(testdouts_0[256]));
    v.set_bit(257, vl::rd(testdouts_0[257]));
    v.set_bit(258, vl::rd(testdouts_0[258]));
    v.set_bit(259, vl::rd(testdouts_0[259]));
    v.set_bit(260, vl::rd(testdouts_0[260]));
    v.set_bit(261, vl::rd(testdouts_0[261]));
    v.set_bit(262, vl::rd(testdouts_0[262]));
    v.set_bit(263, vl::rd(testdouts_0[263]));
    v.set_bit(264, vl::rd(testdouts_0[264]));
    v.set_bit(265, vl::rd(testdouts_0[265]));
    v.set_bit(266, vl::rd(testdouts_0[266]));
    v.set_bit(267, vl::rd(testdouts_0[267]));
    v.set_bit(268, vl::rd(testdouts_0[268]));
    v.set_bit(269, vl::rd(testdouts_0[269]));
    v.set_bit(270, vl::rd(testdouts_0[270]));
    v.set_bit(271, vl::rd(testdouts_0[271]));
    v.set_bit(272, vl::rd(testdouts_0[272]));
    v.set_bit(273, vl::rd(testdouts_0[273]));
    v.set_bit(274, vl::rd(testdouts_0[274]));
    v.set_bit(275, vl::rd(testdouts_0[275]));
    v.set_bit(276, vl::rd(testdouts_0[276]));
    v.set_bit(277, vl::rd(testdouts_0[277]));
    v.set_bit(278, vl::rd(testdouts_0[278]));
    v.set_bit(279, vl::rd(testdouts_0[279]));
    v.set_bit(280, vl::rd(testdouts_0[280]));
    v.set_bit(281, vl::rd(testdouts_0[281]));
    v.set_bit(282, vl::rd(testdouts_0[282]));
    v.set_bit(283, vl::rd(testdouts_0[283]));
    v.set_bit(284, vl::rd(testdouts_0[284]));
    v.set_bit(285, vl::rd(testdouts_0[285]));
    v.set_bit(286, vl::rd(testdouts_0[286]));
    v.set_bit(287, vl::rd(testdouts_0[287]));
    v.set_bit(288, vl::rd(testdouts_0[288]));
    v.set_bit(289, vl::rd(testdouts_0[289]));
    v.set_bit(290, vl::rd(testdouts_0[290]));
    v.set_bit(291, vl::rd(testdouts_0[291]));
    v.set_bit(292, vl::rd(testdouts_0[292]));
    v.set_bit(293, vl::rd(testdouts_0[293]));
    v.set_bit(294, vl::rd(testdouts_0[294]));
    v.set_bit(295, vl::rd(testdouts_0[295]));
    v.set_bit(296, vl::rd(testdouts_0[296]));
    v.set_bit(297, vl::rd(testdouts_0[297]));
    v.set_bit(298, vl::rd(testdouts_0[298]));
    v.set_bit(299, vl::rd(testdouts_0[299]));
    v.set_bit(300, vl::rd(testdouts_0[300]));
    v.set_bit(301, vl::rd(testdouts_0[301]));
    v.set_bit(302, vl::rd(testdouts_0[302]));
    v.set_bit(303, vl::rd(testdouts_0[303]));
    v.set_bit(304, vl::rd(testdouts_0[304]));
    v.set_bit(305, vl::rd(testdouts_0[305]));
    v.set_bit(306, vl::rd(testdouts_0[306]));
    v.set_bit(307, vl::rd(testdouts_0[307]));
    v.set_bit(308, vl::rd(testdouts_0[308]));
    v.set_bit(309, vl::rd(testdouts_0[309]));
    v.set_bit(310, vl::rd(testdouts_0[310]));
    v.set_bit(311, vl::rd(testdouts_0[311]));
    v.set_bit(312, vl::rd(testdouts_0[312]));
    v.set_bit(313, vl::rd(testdouts_0[313]));
    v.set_bit(314, vl::rd(testdouts_0[314]));
    return v;
}

vl::logic<315> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdouts_1() const {
    vl::logic<315> v(0);
    v.set_bit(0, vl::rd(testdouts_1[0]));
    v.set_bit(1, vl::rd(testdouts_1[1]));
    v.set_bit(2, vl::rd(testdouts_1[2]));
    v.set_bit(3, vl::rd(testdouts_1[3]));
    v.set_bit(4, vl::rd(testdouts_1[4]));
    v.set_bit(5, vl::rd(testdouts_1[5]));
    v.set_bit(6, vl::rd(testdouts_1[6]));
    v.set_bit(7, vl::rd(testdouts_1[7]));
    v.set_bit(8, vl::rd(testdouts_1[8]));
    v.set_bit(9, vl::rd(testdouts_1[9]));
    v.set_bit(10, vl::rd(testdouts_1[10]));
    v.set_bit(11, vl::rd(testdouts_1[11]));
    v.set_bit(12, vl::rd(testdouts_1[12]));
    v.set_bit(13, vl::rd(testdouts_1[13]));
    v.set_bit(14, vl::rd(testdouts_1[14]));
    v.set_bit(15, vl::rd(testdouts_1[15]));
    v.set_bit(16, vl::rd(testdouts_1[16]));
    v.set_bit(17, vl::rd(testdouts_1[17]));
    v.set_bit(18, vl::rd(testdouts_1[18]));
    v.set_bit(19, vl::rd(testdouts_1[19]));
    v.set_bit(20, vl::rd(testdouts_1[20]));
    v.set_bit(21, vl::rd(testdouts_1[21]));
    v.set_bit(22, vl::rd(testdouts_1[22]));
    v.set_bit(23, vl::rd(testdouts_1[23]));
    v.set_bit(24, vl::rd(testdouts_1[24]));
    v.set_bit(25, vl::rd(testdouts_1[25]));
    v.set_bit(26, vl::rd(testdouts_1[26]));
    v.set_bit(27, vl::rd(testdouts_1[27]));
    v.set_bit(28, vl::rd(testdouts_1[28]));
    v.set_bit(29, vl::rd(testdouts_1[29]));
    v.set_bit(30, vl::rd(testdouts_1[30]));
    v.set_bit(31, vl::rd(testdouts_1[31]));
    v.set_bit(32, vl::rd(testdouts_1[32]));
    v.set_bit(33, vl::rd(testdouts_1[33]));
    v.set_bit(34, vl::rd(testdouts_1[34]));
    v.set_bit(35, vl::rd(testdouts_1[35]));
    v.set_bit(36, vl::rd(testdouts_1[36]));
    v.set_bit(37, vl::rd(testdouts_1[37]));
    v.set_bit(38, vl::rd(testdouts_1[38]));
    v.set_bit(39, vl::rd(testdouts_1[39]));
    v.set_bit(40, vl::rd(testdouts_1[40]));
    v.set_bit(41, vl::rd(testdouts_1[41]));
    v.set_bit(42, vl::rd(testdouts_1[42]));
    v.set_bit(43, vl::rd(testdouts_1[43]));
    v.set_bit(44, vl::rd(testdouts_1[44]));
    v.set_bit(45, vl::rd(testdouts_1[45]));
    v.set_bit(46, vl::rd(testdouts_1[46]));
    v.set_bit(47, vl::rd(testdouts_1[47]));
    v.set_bit(48, vl::rd(testdouts_1[48]));
    v.set_bit(49, vl::rd(testdouts_1[49]));
    v.set_bit(50, vl::rd(testdouts_1[50]));
    v.set_bit(51, vl::rd(testdouts_1[51]));
    v.set_bit(52, vl::rd(testdouts_1[52]));
    v.set_bit(53, vl::rd(testdouts_1[53]));
    v.set_bit(54, vl::rd(testdouts_1[54]));
    v.set_bit(55, vl::rd(testdouts_1[55]));
    v.set_bit(56, vl::rd(testdouts_1[56]));
    v.set_bit(57, vl::rd(testdouts_1[57]));
    v.set_bit(58, vl::rd(testdouts_1[58]));
    v.set_bit(59, vl::rd(testdouts_1[59]));
    v.set_bit(60, vl::rd(testdouts_1[60]));
    v.set_bit(61, vl::rd(testdouts_1[61]));
    v.set_bit(62, vl::rd(testdouts_1[62]));
    v.set_bit(63, vl::rd(testdouts_1[63]));
    v.set_bit(64, vl::rd(testdouts_1[64]));
    v.set_bit(65, vl::rd(testdouts_1[65]));
    v.set_bit(66, vl::rd(testdouts_1[66]));
    v.set_bit(67, vl::rd(testdouts_1[67]));
    v.set_bit(68, vl::rd(testdouts_1[68]));
    v.set_bit(69, vl::rd(testdouts_1[69]));
    v.set_bit(70, vl::rd(testdouts_1[70]));
    v.set_bit(71, vl::rd(testdouts_1[71]));
    v.set_bit(72, vl::rd(testdouts_1[72]));
    v.set_bit(73, vl::rd(testdouts_1[73]));
    v.set_bit(74, vl::rd(testdouts_1[74]));
    v.set_bit(75, vl::rd(testdouts_1[75]));
    v.set_bit(76, vl::rd(testdouts_1[76]));
    v.set_bit(77, vl::rd(testdouts_1[77]));
    v.set_bit(78, vl::rd(testdouts_1[78]));
    v.set_bit(79, vl::rd(testdouts_1[79]));
    v.set_bit(80, vl::rd(testdouts_1[80]));
    v.set_bit(81, vl::rd(testdouts_1[81]));
    v.set_bit(82, vl::rd(testdouts_1[82]));
    v.set_bit(83, vl::rd(testdouts_1[83]));
    v.set_bit(84, vl::rd(testdouts_1[84]));
    v.set_bit(85, vl::rd(testdouts_1[85]));
    v.set_bit(86, vl::rd(testdouts_1[86]));
    v.set_bit(87, vl::rd(testdouts_1[87]));
    v.set_bit(88, vl::rd(testdouts_1[88]));
    v.set_bit(89, vl::rd(testdouts_1[89]));
    v.set_bit(90, vl::rd(testdouts_1[90]));
    v.set_bit(91, vl::rd(testdouts_1[91]));
    v.set_bit(92, vl::rd(testdouts_1[92]));
    v.set_bit(93, vl::rd(testdouts_1[93]));
    v.set_bit(94, vl::rd(testdouts_1[94]));
    v.set_bit(95, vl::rd(testdouts_1[95]));
    v.set_bit(96, vl::rd(testdouts_1[96]));
    v.set_bit(97, vl::rd(testdouts_1[97]));
    v.set_bit(98, vl::rd(testdouts_1[98]));
    v.set_bit(99, vl::rd(testdouts_1[99]));
    v.set_bit(100, vl::rd(testdouts_1[100]));
    v.set_bit(101, vl::rd(testdouts_1[101]));
    v.set_bit(102, vl::rd(testdouts_1[102]));
    v.set_bit(103, vl::rd(testdouts_1[103]));
    v.set_bit(104, vl::rd(testdouts_1[104]));
    v.set_bit(105, vl::rd(testdouts_1[105]));
    v.set_bit(106, vl::rd(testdouts_1[106]));
    v.set_bit(107, vl::rd(testdouts_1[107]));
    v.set_bit(108, vl::rd(testdouts_1[108]));
    v.set_bit(109, vl::rd(testdouts_1[109]));
    v.set_bit(110, vl::rd(testdouts_1[110]));
    v.set_bit(111, vl::rd(testdouts_1[111]));
    v.set_bit(112, vl::rd(testdouts_1[112]));
    v.set_bit(113, vl::rd(testdouts_1[113]));
    v.set_bit(114, vl::rd(testdouts_1[114]));
    v.set_bit(115, vl::rd(testdouts_1[115]));
    v.set_bit(116, vl::rd(testdouts_1[116]));
    v.set_bit(117, vl::rd(testdouts_1[117]));
    v.set_bit(118, vl::rd(testdouts_1[118]));
    v.set_bit(119, vl::rd(testdouts_1[119]));
    v.set_bit(120, vl::rd(testdouts_1[120]));
    v.set_bit(121, vl::rd(testdouts_1[121]));
    v.set_bit(122, vl::rd(testdouts_1[122]));
    v.set_bit(123, vl::rd(testdouts_1[123]));
    v.set_bit(124, vl::rd(testdouts_1[124]));
    v.set_bit(125, vl::rd(testdouts_1[125]));
    v.set_bit(126, vl::rd(testdouts_1[126]));
    v.set_bit(127, vl::rd(testdouts_1[127]));
    v.set_bit(128, vl::rd(testdouts_1[128]));
    v.set_bit(129, vl::rd(testdouts_1[129]));
    v.set_bit(130, vl::rd(testdouts_1[130]));
    v.set_bit(131, vl::rd(testdouts_1[131]));
    v.set_bit(132, vl::rd(testdouts_1[132]));
    v.set_bit(133, vl::rd(testdouts_1[133]));
    v.set_bit(134, vl::rd(testdouts_1[134]));
    v.set_bit(135, vl::rd(testdouts_1[135]));
    v.set_bit(136, vl::rd(testdouts_1[136]));
    v.set_bit(137, vl::rd(testdouts_1[137]));
    v.set_bit(138, vl::rd(testdouts_1[138]));
    v.set_bit(139, vl::rd(testdouts_1[139]));
    v.set_bit(140, vl::rd(testdouts_1[140]));
    v.set_bit(141, vl::rd(testdouts_1[141]));
    v.set_bit(142, vl::rd(testdouts_1[142]));
    v.set_bit(143, vl::rd(testdouts_1[143]));
    v.set_bit(144, vl::rd(testdouts_1[144]));
    v.set_bit(145, vl::rd(testdouts_1[145]));
    v.set_bit(146, vl::rd(testdouts_1[146]));
    v.set_bit(147, vl::rd(testdouts_1[147]));
    v.set_bit(148, vl::rd(testdouts_1[148]));
    v.set_bit(149, vl::rd(testdouts_1[149]));
    v.set_bit(150, vl::rd(testdouts_1[150]));
    v.set_bit(151, vl::rd(testdouts_1[151]));
    v.set_bit(152, vl::rd(testdouts_1[152]));
    v.set_bit(153, vl::rd(testdouts_1[153]));
    v.set_bit(154, vl::rd(testdouts_1[154]));
    v.set_bit(155, vl::rd(testdouts_1[155]));
    v.set_bit(156, vl::rd(testdouts_1[156]));
    v.set_bit(157, vl::rd(testdouts_1[157]));
    v.set_bit(158, vl::rd(testdouts_1[158]));
    v.set_bit(159, vl::rd(testdouts_1[159]));
    v.set_bit(160, vl::rd(testdouts_1[160]));
    v.set_bit(161, vl::rd(testdouts_1[161]));
    v.set_bit(162, vl::rd(testdouts_1[162]));
    v.set_bit(163, vl::rd(testdouts_1[163]));
    v.set_bit(164, vl::rd(testdouts_1[164]));
    v.set_bit(165, vl::rd(testdouts_1[165]));
    v.set_bit(166, vl::rd(testdouts_1[166]));
    v.set_bit(167, vl::rd(testdouts_1[167]));
    v.set_bit(168, vl::rd(testdouts_1[168]));
    v.set_bit(169, vl::rd(testdouts_1[169]));
    v.set_bit(170, vl::rd(testdouts_1[170]));
    v.set_bit(171, vl::rd(testdouts_1[171]));
    v.set_bit(172, vl::rd(testdouts_1[172]));
    v.set_bit(173, vl::rd(testdouts_1[173]));
    v.set_bit(174, vl::rd(testdouts_1[174]));
    v.set_bit(175, vl::rd(testdouts_1[175]));
    v.set_bit(176, vl::rd(testdouts_1[176]));
    v.set_bit(177, vl::rd(testdouts_1[177]));
    v.set_bit(178, vl::rd(testdouts_1[178]));
    v.set_bit(179, vl::rd(testdouts_1[179]));
    v.set_bit(180, vl::rd(testdouts_1[180]));
    v.set_bit(181, vl::rd(testdouts_1[181]));
    v.set_bit(182, vl::rd(testdouts_1[182]));
    v.set_bit(183, vl::rd(testdouts_1[183]));
    v.set_bit(184, vl::rd(testdouts_1[184]));
    v.set_bit(185, vl::rd(testdouts_1[185]));
    v.set_bit(186, vl::rd(testdouts_1[186]));
    v.set_bit(187, vl::rd(testdouts_1[187]));
    v.set_bit(188, vl::rd(testdouts_1[188]));
    v.set_bit(189, vl::rd(testdouts_1[189]));
    v.set_bit(190, vl::rd(testdouts_1[190]));
    v.set_bit(191, vl::rd(testdouts_1[191]));
    v.set_bit(192, vl::rd(testdouts_1[192]));
    v.set_bit(193, vl::rd(testdouts_1[193]));
    v.set_bit(194, vl::rd(testdouts_1[194]));
    v.set_bit(195, vl::rd(testdouts_1[195]));
    v.set_bit(196, vl::rd(testdouts_1[196]));
    v.set_bit(197, vl::rd(testdouts_1[197]));
    v.set_bit(198, vl::rd(testdouts_1[198]));
    v.set_bit(199, vl::rd(testdouts_1[199]));
    v.set_bit(200, vl::rd(testdouts_1[200]));
    v.set_bit(201, vl::rd(testdouts_1[201]));
    v.set_bit(202, vl::rd(testdouts_1[202]));
    v.set_bit(203, vl::rd(testdouts_1[203]));
    v.set_bit(204, vl::rd(testdouts_1[204]));
    v.set_bit(205, vl::rd(testdouts_1[205]));
    v.set_bit(206, vl::rd(testdouts_1[206]));
    v.set_bit(207, vl::rd(testdouts_1[207]));
    v.set_bit(208, vl::rd(testdouts_1[208]));
    v.set_bit(209, vl::rd(testdouts_1[209]));
    v.set_bit(210, vl::rd(testdouts_1[210]));
    v.set_bit(211, vl::rd(testdouts_1[211]));
    v.set_bit(212, vl::rd(testdouts_1[212]));
    v.set_bit(213, vl::rd(testdouts_1[213]));
    v.set_bit(214, vl::rd(testdouts_1[214]));
    v.set_bit(215, vl::rd(testdouts_1[215]));
    v.set_bit(216, vl::rd(testdouts_1[216]));
    v.set_bit(217, vl::rd(testdouts_1[217]));
    v.set_bit(218, vl::rd(testdouts_1[218]));
    v.set_bit(219, vl::rd(testdouts_1[219]));
    v.set_bit(220, vl::rd(testdouts_1[220]));
    v.set_bit(221, vl::rd(testdouts_1[221]));
    v.set_bit(222, vl::rd(testdouts_1[222]));
    v.set_bit(223, vl::rd(testdouts_1[223]));
    v.set_bit(224, vl::rd(testdouts_1[224]));
    v.set_bit(225, vl::rd(testdouts_1[225]));
    v.set_bit(226, vl::rd(testdouts_1[226]));
    v.set_bit(227, vl::rd(testdouts_1[227]));
    v.set_bit(228, vl::rd(testdouts_1[228]));
    v.set_bit(229, vl::rd(testdouts_1[229]));
    v.set_bit(230, vl::rd(testdouts_1[230]));
    v.set_bit(231, vl::rd(testdouts_1[231]));
    v.set_bit(232, vl::rd(testdouts_1[232]));
    v.set_bit(233, vl::rd(testdouts_1[233]));
    v.set_bit(234, vl::rd(testdouts_1[234]));
    v.set_bit(235, vl::rd(testdouts_1[235]));
    v.set_bit(236, vl::rd(testdouts_1[236]));
    v.set_bit(237, vl::rd(testdouts_1[237]));
    v.set_bit(238, vl::rd(testdouts_1[238]));
    v.set_bit(239, vl::rd(testdouts_1[239]));
    v.set_bit(240, vl::rd(testdouts_1[240]));
    v.set_bit(241, vl::rd(testdouts_1[241]));
    v.set_bit(242, vl::rd(testdouts_1[242]));
    v.set_bit(243, vl::rd(testdouts_1[243]));
    v.set_bit(244, vl::rd(testdouts_1[244]));
    v.set_bit(245, vl::rd(testdouts_1[245]));
    v.set_bit(246, vl::rd(testdouts_1[246]));
    v.set_bit(247, vl::rd(testdouts_1[247]));
    v.set_bit(248, vl::rd(testdouts_1[248]));
    v.set_bit(249, vl::rd(testdouts_1[249]));
    v.set_bit(250, vl::rd(testdouts_1[250]));
    v.set_bit(251, vl::rd(testdouts_1[251]));
    v.set_bit(252, vl::rd(testdouts_1[252]));
    v.set_bit(253, vl::rd(testdouts_1[253]));
    v.set_bit(254, vl::rd(testdouts_1[254]));
    v.set_bit(255, vl::rd(testdouts_1[255]));
    v.set_bit(256, vl::rd(testdouts_1[256]));
    v.set_bit(257, vl::rd(testdouts_1[257]));
    v.set_bit(258, vl::rd(testdouts_1[258]));
    v.set_bit(259, vl::rd(testdouts_1[259]));
    v.set_bit(260, vl::rd(testdouts_1[260]));
    v.set_bit(261, vl::rd(testdouts_1[261]));
    v.set_bit(262, vl::rd(testdouts_1[262]));
    v.set_bit(263, vl::rd(testdouts_1[263]));
    v.set_bit(264, vl::rd(testdouts_1[264]));
    v.set_bit(265, vl::rd(testdouts_1[265]));
    v.set_bit(266, vl::rd(testdouts_1[266]));
    v.set_bit(267, vl::rd(testdouts_1[267]));
    v.set_bit(268, vl::rd(testdouts_1[268]));
    v.set_bit(269, vl::rd(testdouts_1[269]));
    v.set_bit(270, vl::rd(testdouts_1[270]));
    v.set_bit(271, vl::rd(testdouts_1[271]));
    v.set_bit(272, vl::rd(testdouts_1[272]));
    v.set_bit(273, vl::rd(testdouts_1[273]));
    v.set_bit(274, vl::rd(testdouts_1[274]));
    v.set_bit(275, vl::rd(testdouts_1[275]));
    v.set_bit(276, vl::rd(testdouts_1[276]));
    v.set_bit(277, vl::rd(testdouts_1[277]));
    v.set_bit(278, vl::rd(testdouts_1[278]));
    v.set_bit(279, vl::rd(testdouts_1[279]));
    v.set_bit(280, vl::rd(testdouts_1[280]));
    v.set_bit(281, vl::rd(testdouts_1[281]));
    v.set_bit(282, vl::rd(testdouts_1[282]));
    v.set_bit(283, vl::rd(testdouts_1[283]));
    v.set_bit(284, vl::rd(testdouts_1[284]));
    v.set_bit(285, vl::rd(testdouts_1[285]));
    v.set_bit(286, vl::rd(testdouts_1[286]));
    v.set_bit(287, vl::rd(testdouts_1[287]));
    v.set_bit(288, vl::rd(testdouts_1[288]));
    v.set_bit(289, vl::rd(testdouts_1[289]));
    v.set_bit(290, vl::rd(testdouts_1[290]));
    v.set_bit(291, vl::rd(testdouts_1[291]));
    v.set_bit(292, vl::rd(testdouts_1[292]));
    v.set_bit(293, vl::rd(testdouts_1[293]));
    v.set_bit(294, vl::rd(testdouts_1[294]));
    v.set_bit(295, vl::rd(testdouts_1[295]));
    v.set_bit(296, vl::rd(testdouts_1[296]));
    v.set_bit(297, vl::rd(testdouts_1[297]));
    v.set_bit(298, vl::rd(testdouts_1[298]));
    v.set_bit(299, vl::rd(testdouts_1[299]));
    v.set_bit(300, vl::rd(testdouts_1[300]));
    v.set_bit(301, vl::rd(testdouts_1[301]));
    v.set_bit(302, vl::rd(testdouts_1[302]));
    v.set_bit(303, vl::rd(testdouts_1[303]));
    v.set_bit(304, vl::rd(testdouts_1[304]));
    v.set_bit(305, vl::rd(testdouts_1[305]));
    v.set_bit(306, vl::rd(testdouts_1[306]));
    v.set_bit(307, vl::rd(testdouts_1[307]));
    v.set_bit(308, vl::rd(testdouts_1[308]));
    v.set_bit(309, vl::rd(testdouts_1[309]));
    v.set_bit(310, vl::rd(testdouts_1[310]));
    v.set_bit(311, vl::rd(testdouts_1[311]));
    v.set_bit(312, vl::rd(testdouts_1[312]));
    v.set_bit(313, vl::rd(testdouts_1[313]));
    v.set_bit(314, vl::rd(testdouts_1[314]));
    return v;
}

vl::logic<315> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdouts_2() const {
    vl::logic<315> v(0);
    v.set_bit(0, vl::rd(testdouts_2[0]));
    v.set_bit(1, vl::rd(testdouts_2[1]));
    v.set_bit(2, vl::rd(testdouts_2[2]));
    v.set_bit(3, vl::rd(testdouts_2[3]));
    v.set_bit(4, vl::rd(testdouts_2[4]));
    v.set_bit(5, vl::rd(testdouts_2[5]));
    v.set_bit(6, vl::rd(testdouts_2[6]));
    v.set_bit(7, vl::rd(testdouts_2[7]));
    v.set_bit(8, vl::rd(testdouts_2[8]));
    v.set_bit(9, vl::rd(testdouts_2[9]));
    v.set_bit(10, vl::rd(testdouts_2[10]));
    v.set_bit(11, vl::rd(testdouts_2[11]));
    v.set_bit(12, vl::rd(testdouts_2[12]));
    v.set_bit(13, vl::rd(testdouts_2[13]));
    v.set_bit(14, vl::rd(testdouts_2[14]));
    v.set_bit(15, vl::rd(testdouts_2[15]));
    v.set_bit(16, vl::rd(testdouts_2[16]));
    v.set_bit(17, vl::rd(testdouts_2[17]));
    v.set_bit(18, vl::rd(testdouts_2[18]));
    v.set_bit(19, vl::rd(testdouts_2[19]));
    v.set_bit(20, vl::rd(testdouts_2[20]));
    v.set_bit(21, vl::rd(testdouts_2[21]));
    v.set_bit(22, vl::rd(testdouts_2[22]));
    v.set_bit(23, vl::rd(testdouts_2[23]));
    v.set_bit(24, vl::rd(testdouts_2[24]));
    v.set_bit(25, vl::rd(testdouts_2[25]));
    v.set_bit(26, vl::rd(testdouts_2[26]));
    v.set_bit(27, vl::rd(testdouts_2[27]));
    v.set_bit(28, vl::rd(testdouts_2[28]));
    v.set_bit(29, vl::rd(testdouts_2[29]));
    v.set_bit(30, vl::rd(testdouts_2[30]));
    v.set_bit(31, vl::rd(testdouts_2[31]));
    v.set_bit(32, vl::rd(testdouts_2[32]));
    v.set_bit(33, vl::rd(testdouts_2[33]));
    v.set_bit(34, vl::rd(testdouts_2[34]));
    v.set_bit(35, vl::rd(testdouts_2[35]));
    v.set_bit(36, vl::rd(testdouts_2[36]));
    v.set_bit(37, vl::rd(testdouts_2[37]));
    v.set_bit(38, vl::rd(testdouts_2[38]));
    v.set_bit(39, vl::rd(testdouts_2[39]));
    v.set_bit(40, vl::rd(testdouts_2[40]));
    v.set_bit(41, vl::rd(testdouts_2[41]));
    v.set_bit(42, vl::rd(testdouts_2[42]));
    v.set_bit(43, vl::rd(testdouts_2[43]));
    v.set_bit(44, vl::rd(testdouts_2[44]));
    v.set_bit(45, vl::rd(testdouts_2[45]));
    v.set_bit(46, vl::rd(testdouts_2[46]));
    v.set_bit(47, vl::rd(testdouts_2[47]));
    v.set_bit(48, vl::rd(testdouts_2[48]));
    v.set_bit(49, vl::rd(testdouts_2[49]));
    v.set_bit(50, vl::rd(testdouts_2[50]));
    v.set_bit(51, vl::rd(testdouts_2[51]));
    v.set_bit(52, vl::rd(testdouts_2[52]));
    v.set_bit(53, vl::rd(testdouts_2[53]));
    v.set_bit(54, vl::rd(testdouts_2[54]));
    v.set_bit(55, vl::rd(testdouts_2[55]));
    v.set_bit(56, vl::rd(testdouts_2[56]));
    v.set_bit(57, vl::rd(testdouts_2[57]));
    v.set_bit(58, vl::rd(testdouts_2[58]));
    v.set_bit(59, vl::rd(testdouts_2[59]));
    v.set_bit(60, vl::rd(testdouts_2[60]));
    v.set_bit(61, vl::rd(testdouts_2[61]));
    v.set_bit(62, vl::rd(testdouts_2[62]));
    v.set_bit(63, vl::rd(testdouts_2[63]));
    v.set_bit(64, vl::rd(testdouts_2[64]));
    v.set_bit(65, vl::rd(testdouts_2[65]));
    v.set_bit(66, vl::rd(testdouts_2[66]));
    v.set_bit(67, vl::rd(testdouts_2[67]));
    v.set_bit(68, vl::rd(testdouts_2[68]));
    v.set_bit(69, vl::rd(testdouts_2[69]));
    v.set_bit(70, vl::rd(testdouts_2[70]));
    v.set_bit(71, vl::rd(testdouts_2[71]));
    v.set_bit(72, vl::rd(testdouts_2[72]));
    v.set_bit(73, vl::rd(testdouts_2[73]));
    v.set_bit(74, vl::rd(testdouts_2[74]));
    v.set_bit(75, vl::rd(testdouts_2[75]));
    v.set_bit(76, vl::rd(testdouts_2[76]));
    v.set_bit(77, vl::rd(testdouts_2[77]));
    v.set_bit(78, vl::rd(testdouts_2[78]));
    v.set_bit(79, vl::rd(testdouts_2[79]));
    v.set_bit(80, vl::rd(testdouts_2[80]));
    v.set_bit(81, vl::rd(testdouts_2[81]));
    v.set_bit(82, vl::rd(testdouts_2[82]));
    v.set_bit(83, vl::rd(testdouts_2[83]));
    v.set_bit(84, vl::rd(testdouts_2[84]));
    v.set_bit(85, vl::rd(testdouts_2[85]));
    v.set_bit(86, vl::rd(testdouts_2[86]));
    v.set_bit(87, vl::rd(testdouts_2[87]));
    v.set_bit(88, vl::rd(testdouts_2[88]));
    v.set_bit(89, vl::rd(testdouts_2[89]));
    v.set_bit(90, vl::rd(testdouts_2[90]));
    v.set_bit(91, vl::rd(testdouts_2[91]));
    v.set_bit(92, vl::rd(testdouts_2[92]));
    v.set_bit(93, vl::rd(testdouts_2[93]));
    v.set_bit(94, vl::rd(testdouts_2[94]));
    v.set_bit(95, vl::rd(testdouts_2[95]));
    v.set_bit(96, vl::rd(testdouts_2[96]));
    v.set_bit(97, vl::rd(testdouts_2[97]));
    v.set_bit(98, vl::rd(testdouts_2[98]));
    v.set_bit(99, vl::rd(testdouts_2[99]));
    v.set_bit(100, vl::rd(testdouts_2[100]));
    v.set_bit(101, vl::rd(testdouts_2[101]));
    v.set_bit(102, vl::rd(testdouts_2[102]));
    v.set_bit(103, vl::rd(testdouts_2[103]));
    v.set_bit(104, vl::rd(testdouts_2[104]));
    v.set_bit(105, vl::rd(testdouts_2[105]));
    v.set_bit(106, vl::rd(testdouts_2[106]));
    v.set_bit(107, vl::rd(testdouts_2[107]));
    v.set_bit(108, vl::rd(testdouts_2[108]));
    v.set_bit(109, vl::rd(testdouts_2[109]));
    v.set_bit(110, vl::rd(testdouts_2[110]));
    v.set_bit(111, vl::rd(testdouts_2[111]));
    v.set_bit(112, vl::rd(testdouts_2[112]));
    v.set_bit(113, vl::rd(testdouts_2[113]));
    v.set_bit(114, vl::rd(testdouts_2[114]));
    v.set_bit(115, vl::rd(testdouts_2[115]));
    v.set_bit(116, vl::rd(testdouts_2[116]));
    v.set_bit(117, vl::rd(testdouts_2[117]));
    v.set_bit(118, vl::rd(testdouts_2[118]));
    v.set_bit(119, vl::rd(testdouts_2[119]));
    v.set_bit(120, vl::rd(testdouts_2[120]));
    v.set_bit(121, vl::rd(testdouts_2[121]));
    v.set_bit(122, vl::rd(testdouts_2[122]));
    v.set_bit(123, vl::rd(testdouts_2[123]));
    v.set_bit(124, vl::rd(testdouts_2[124]));
    v.set_bit(125, vl::rd(testdouts_2[125]));
    v.set_bit(126, vl::rd(testdouts_2[126]));
    v.set_bit(127, vl::rd(testdouts_2[127]));
    v.set_bit(128, vl::rd(testdouts_2[128]));
    v.set_bit(129, vl::rd(testdouts_2[129]));
    v.set_bit(130, vl::rd(testdouts_2[130]));
    v.set_bit(131, vl::rd(testdouts_2[131]));
    v.set_bit(132, vl::rd(testdouts_2[132]));
    v.set_bit(133, vl::rd(testdouts_2[133]));
    v.set_bit(134, vl::rd(testdouts_2[134]));
    v.set_bit(135, vl::rd(testdouts_2[135]));
    v.set_bit(136, vl::rd(testdouts_2[136]));
    v.set_bit(137, vl::rd(testdouts_2[137]));
    v.set_bit(138, vl::rd(testdouts_2[138]));
    v.set_bit(139, vl::rd(testdouts_2[139]));
    v.set_bit(140, vl::rd(testdouts_2[140]));
    v.set_bit(141, vl::rd(testdouts_2[141]));
    v.set_bit(142, vl::rd(testdouts_2[142]));
    v.set_bit(143, vl::rd(testdouts_2[143]));
    v.set_bit(144, vl::rd(testdouts_2[144]));
    v.set_bit(145, vl::rd(testdouts_2[145]));
    v.set_bit(146, vl::rd(testdouts_2[146]));
    v.set_bit(147, vl::rd(testdouts_2[147]));
    v.set_bit(148, vl::rd(testdouts_2[148]));
    v.set_bit(149, vl::rd(testdouts_2[149]));
    v.set_bit(150, vl::rd(testdouts_2[150]));
    v.set_bit(151, vl::rd(testdouts_2[151]));
    v.set_bit(152, vl::rd(testdouts_2[152]));
    v.set_bit(153, vl::rd(testdouts_2[153]));
    v.set_bit(154, vl::rd(testdouts_2[154]));
    v.set_bit(155, vl::rd(testdouts_2[155]));
    v.set_bit(156, vl::rd(testdouts_2[156]));
    v.set_bit(157, vl::rd(testdouts_2[157]));
    v.set_bit(158, vl::rd(testdouts_2[158]));
    v.set_bit(159, vl::rd(testdouts_2[159]));
    v.set_bit(160, vl::rd(testdouts_2[160]));
    v.set_bit(161, vl::rd(testdouts_2[161]));
    v.set_bit(162, vl::rd(testdouts_2[162]));
    v.set_bit(163, vl::rd(testdouts_2[163]));
    v.set_bit(164, vl::rd(testdouts_2[164]));
    v.set_bit(165, vl::rd(testdouts_2[165]));
    v.set_bit(166, vl::rd(testdouts_2[166]));
    v.set_bit(167, vl::rd(testdouts_2[167]));
    v.set_bit(168, vl::rd(testdouts_2[168]));
    v.set_bit(169, vl::rd(testdouts_2[169]));
    v.set_bit(170, vl::rd(testdouts_2[170]));
    v.set_bit(171, vl::rd(testdouts_2[171]));
    v.set_bit(172, vl::rd(testdouts_2[172]));
    v.set_bit(173, vl::rd(testdouts_2[173]));
    v.set_bit(174, vl::rd(testdouts_2[174]));
    v.set_bit(175, vl::rd(testdouts_2[175]));
    v.set_bit(176, vl::rd(testdouts_2[176]));
    v.set_bit(177, vl::rd(testdouts_2[177]));
    v.set_bit(178, vl::rd(testdouts_2[178]));
    v.set_bit(179, vl::rd(testdouts_2[179]));
    v.set_bit(180, vl::rd(testdouts_2[180]));
    v.set_bit(181, vl::rd(testdouts_2[181]));
    v.set_bit(182, vl::rd(testdouts_2[182]));
    v.set_bit(183, vl::rd(testdouts_2[183]));
    v.set_bit(184, vl::rd(testdouts_2[184]));
    v.set_bit(185, vl::rd(testdouts_2[185]));
    v.set_bit(186, vl::rd(testdouts_2[186]));
    v.set_bit(187, vl::rd(testdouts_2[187]));
    v.set_bit(188, vl::rd(testdouts_2[188]));
    v.set_bit(189, vl::rd(testdouts_2[189]));
    v.set_bit(190, vl::rd(testdouts_2[190]));
    v.set_bit(191, vl::rd(testdouts_2[191]));
    v.set_bit(192, vl::rd(testdouts_2[192]));
    v.set_bit(193, vl::rd(testdouts_2[193]));
    v.set_bit(194, vl::rd(testdouts_2[194]));
    v.set_bit(195, vl::rd(testdouts_2[195]));
    v.set_bit(196, vl::rd(testdouts_2[196]));
    v.set_bit(197, vl::rd(testdouts_2[197]));
    v.set_bit(198, vl::rd(testdouts_2[198]));
    v.set_bit(199, vl::rd(testdouts_2[199]));
    v.set_bit(200, vl::rd(testdouts_2[200]));
    v.set_bit(201, vl::rd(testdouts_2[201]));
    v.set_bit(202, vl::rd(testdouts_2[202]));
    v.set_bit(203, vl::rd(testdouts_2[203]));
    v.set_bit(204, vl::rd(testdouts_2[204]));
    v.set_bit(205, vl::rd(testdouts_2[205]));
    v.set_bit(206, vl::rd(testdouts_2[206]));
    v.set_bit(207, vl::rd(testdouts_2[207]));
    v.set_bit(208, vl::rd(testdouts_2[208]));
    v.set_bit(209, vl::rd(testdouts_2[209]));
    v.set_bit(210, vl::rd(testdouts_2[210]));
    v.set_bit(211, vl::rd(testdouts_2[211]));
    v.set_bit(212, vl::rd(testdouts_2[212]));
    v.set_bit(213, vl::rd(testdouts_2[213]));
    v.set_bit(214, vl::rd(testdouts_2[214]));
    v.set_bit(215, vl::rd(testdouts_2[215]));
    v.set_bit(216, vl::rd(testdouts_2[216]));
    v.set_bit(217, vl::rd(testdouts_2[217]));
    v.set_bit(218, vl::rd(testdouts_2[218]));
    v.set_bit(219, vl::rd(testdouts_2[219]));
    v.set_bit(220, vl::rd(testdouts_2[220]));
    v.set_bit(221, vl::rd(testdouts_2[221]));
    v.set_bit(222, vl::rd(testdouts_2[222]));
    v.set_bit(223, vl::rd(testdouts_2[223]));
    v.set_bit(224, vl::rd(testdouts_2[224]));
    v.set_bit(225, vl::rd(testdouts_2[225]));
    v.set_bit(226, vl::rd(testdouts_2[226]));
    v.set_bit(227, vl::rd(testdouts_2[227]));
    v.set_bit(228, vl::rd(testdouts_2[228]));
    v.set_bit(229, vl::rd(testdouts_2[229]));
    v.set_bit(230, vl::rd(testdouts_2[230]));
    v.set_bit(231, vl::rd(testdouts_2[231]));
    v.set_bit(232, vl::rd(testdouts_2[232]));
    v.set_bit(233, vl::rd(testdouts_2[233]));
    v.set_bit(234, vl::rd(testdouts_2[234]));
    v.set_bit(235, vl::rd(testdouts_2[235]));
    v.set_bit(236, vl::rd(testdouts_2[236]));
    v.set_bit(237, vl::rd(testdouts_2[237]));
    v.set_bit(238, vl::rd(testdouts_2[238]));
    v.set_bit(239, vl::rd(testdouts_2[239]));
    v.set_bit(240, vl::rd(testdouts_2[240]));
    v.set_bit(241, vl::rd(testdouts_2[241]));
    v.set_bit(242, vl::rd(testdouts_2[242]));
    v.set_bit(243, vl::rd(testdouts_2[243]));
    v.set_bit(244, vl::rd(testdouts_2[244]));
    v.set_bit(245, vl::rd(testdouts_2[245]));
    v.set_bit(246, vl::rd(testdouts_2[246]));
    v.set_bit(247, vl::rd(testdouts_2[247]));
    v.set_bit(248, vl::rd(testdouts_2[248]));
    v.set_bit(249, vl::rd(testdouts_2[249]));
    v.set_bit(250, vl::rd(testdouts_2[250]));
    v.set_bit(251, vl::rd(testdouts_2[251]));
    v.set_bit(252, vl::rd(testdouts_2[252]));
    v.set_bit(253, vl::rd(testdouts_2[253]));
    v.set_bit(254, vl::rd(testdouts_2[254]));
    v.set_bit(255, vl::rd(testdouts_2[255]));
    v.set_bit(256, vl::rd(testdouts_2[256]));
    v.set_bit(257, vl::rd(testdouts_2[257]));
    v.set_bit(258, vl::rd(testdouts_2[258]));
    v.set_bit(259, vl::rd(testdouts_2[259]));
    v.set_bit(260, vl::rd(testdouts_2[260]));
    v.set_bit(261, vl::rd(testdouts_2[261]));
    v.set_bit(262, vl::rd(testdouts_2[262]));
    v.set_bit(263, vl::rd(testdouts_2[263]));
    v.set_bit(264, vl::rd(testdouts_2[264]));
    v.set_bit(265, vl::rd(testdouts_2[265]));
    v.set_bit(266, vl::rd(testdouts_2[266]));
    v.set_bit(267, vl::rd(testdouts_2[267]));
    v.set_bit(268, vl::rd(testdouts_2[268]));
    v.set_bit(269, vl::rd(testdouts_2[269]));
    v.set_bit(270, vl::rd(testdouts_2[270]));
    v.set_bit(271, vl::rd(testdouts_2[271]));
    v.set_bit(272, vl::rd(testdouts_2[272]));
    v.set_bit(273, vl::rd(testdouts_2[273]));
    v.set_bit(274, vl::rd(testdouts_2[274]));
    v.set_bit(275, vl::rd(testdouts_2[275]));
    v.set_bit(276, vl::rd(testdouts_2[276]));
    v.set_bit(277, vl::rd(testdouts_2[277]));
    v.set_bit(278, vl::rd(testdouts_2[278]));
    v.set_bit(279, vl::rd(testdouts_2[279]));
    v.set_bit(280, vl::rd(testdouts_2[280]));
    v.set_bit(281, vl::rd(testdouts_2[281]));
    v.set_bit(282, vl::rd(testdouts_2[282]));
    v.set_bit(283, vl::rd(testdouts_2[283]));
    v.set_bit(284, vl::rd(testdouts_2[284]));
    v.set_bit(285, vl::rd(testdouts_2[285]));
    v.set_bit(286, vl::rd(testdouts_2[286]));
    v.set_bit(287, vl::rd(testdouts_2[287]));
    v.set_bit(288, vl::rd(testdouts_2[288]));
    v.set_bit(289, vl::rd(testdouts_2[289]));
    v.set_bit(290, vl::rd(testdouts_2[290]));
    v.set_bit(291, vl::rd(testdouts_2[291]));
    v.set_bit(292, vl::rd(testdouts_2[292]));
    v.set_bit(293, vl::rd(testdouts_2[293]));
    v.set_bit(294, vl::rd(testdouts_2[294]));
    v.set_bit(295, vl::rd(testdouts_2[295]));
    v.set_bit(296, vl::rd(testdouts_2[296]));
    v.set_bit(297, vl::rd(testdouts_2[297]));
    v.set_bit(298, vl::rd(testdouts_2[298]));
    v.set_bit(299, vl::rd(testdouts_2[299]));
    v.set_bit(300, vl::rd(testdouts_2[300]));
    v.set_bit(301, vl::rd(testdouts_2[301]));
    v.set_bit(302, vl::rd(testdouts_2[302]));
    v.set_bit(303, vl::rd(testdouts_2[303]));
    v.set_bit(304, vl::rd(testdouts_2[304]));
    v.set_bit(305, vl::rd(testdouts_2[305]));
    v.set_bit(306, vl::rd(testdouts_2[306]));
    v.set_bit(307, vl::rd(testdouts_2[307]));
    v.set_bit(308, vl::rd(testdouts_2[308]));
    v.set_bit(309, vl::rd(testdouts_2[309]));
    v.set_bit(310, vl::rd(testdouts_2[310]));
    v.set_bit(311, vl::rd(testdouts_2[311]));
    v.set_bit(312, vl::rd(testdouts_2[312]));
    v.set_bit(313, vl::rd(testdouts_2[313]));
    v.set_bit(314, vl::rd(testdouts_2[314]));
    return v;
}

vl::logic<315> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdouts_3() const {
    vl::logic<315> v(0);
    v.set_bit(0, vl::rd(testdouts_3[0]));
    v.set_bit(1, vl::rd(testdouts_3[1]));
    v.set_bit(2, vl::rd(testdouts_3[2]));
    v.set_bit(3, vl::rd(testdouts_3[3]));
    v.set_bit(4, vl::rd(testdouts_3[4]));
    v.set_bit(5, vl::rd(testdouts_3[5]));
    v.set_bit(6, vl::rd(testdouts_3[6]));
    v.set_bit(7, vl::rd(testdouts_3[7]));
    v.set_bit(8, vl::rd(testdouts_3[8]));
    v.set_bit(9, vl::rd(testdouts_3[9]));
    v.set_bit(10, vl::rd(testdouts_3[10]));
    v.set_bit(11, vl::rd(testdouts_3[11]));
    v.set_bit(12, vl::rd(testdouts_3[12]));
    v.set_bit(13, vl::rd(testdouts_3[13]));
    v.set_bit(14, vl::rd(testdouts_3[14]));
    v.set_bit(15, vl::rd(testdouts_3[15]));
    v.set_bit(16, vl::rd(testdouts_3[16]));
    v.set_bit(17, vl::rd(testdouts_3[17]));
    v.set_bit(18, vl::rd(testdouts_3[18]));
    v.set_bit(19, vl::rd(testdouts_3[19]));
    v.set_bit(20, vl::rd(testdouts_3[20]));
    v.set_bit(21, vl::rd(testdouts_3[21]));
    v.set_bit(22, vl::rd(testdouts_3[22]));
    v.set_bit(23, vl::rd(testdouts_3[23]));
    v.set_bit(24, vl::rd(testdouts_3[24]));
    v.set_bit(25, vl::rd(testdouts_3[25]));
    v.set_bit(26, vl::rd(testdouts_3[26]));
    v.set_bit(27, vl::rd(testdouts_3[27]));
    v.set_bit(28, vl::rd(testdouts_3[28]));
    v.set_bit(29, vl::rd(testdouts_3[29]));
    v.set_bit(30, vl::rd(testdouts_3[30]));
    v.set_bit(31, vl::rd(testdouts_3[31]));
    v.set_bit(32, vl::rd(testdouts_3[32]));
    v.set_bit(33, vl::rd(testdouts_3[33]));
    v.set_bit(34, vl::rd(testdouts_3[34]));
    v.set_bit(35, vl::rd(testdouts_3[35]));
    v.set_bit(36, vl::rd(testdouts_3[36]));
    v.set_bit(37, vl::rd(testdouts_3[37]));
    v.set_bit(38, vl::rd(testdouts_3[38]));
    v.set_bit(39, vl::rd(testdouts_3[39]));
    v.set_bit(40, vl::rd(testdouts_3[40]));
    v.set_bit(41, vl::rd(testdouts_3[41]));
    v.set_bit(42, vl::rd(testdouts_3[42]));
    v.set_bit(43, vl::rd(testdouts_3[43]));
    v.set_bit(44, vl::rd(testdouts_3[44]));
    v.set_bit(45, vl::rd(testdouts_3[45]));
    v.set_bit(46, vl::rd(testdouts_3[46]));
    v.set_bit(47, vl::rd(testdouts_3[47]));
    v.set_bit(48, vl::rd(testdouts_3[48]));
    v.set_bit(49, vl::rd(testdouts_3[49]));
    v.set_bit(50, vl::rd(testdouts_3[50]));
    v.set_bit(51, vl::rd(testdouts_3[51]));
    v.set_bit(52, vl::rd(testdouts_3[52]));
    v.set_bit(53, vl::rd(testdouts_3[53]));
    v.set_bit(54, vl::rd(testdouts_3[54]));
    v.set_bit(55, vl::rd(testdouts_3[55]));
    v.set_bit(56, vl::rd(testdouts_3[56]));
    v.set_bit(57, vl::rd(testdouts_3[57]));
    v.set_bit(58, vl::rd(testdouts_3[58]));
    v.set_bit(59, vl::rd(testdouts_3[59]));
    v.set_bit(60, vl::rd(testdouts_3[60]));
    v.set_bit(61, vl::rd(testdouts_3[61]));
    v.set_bit(62, vl::rd(testdouts_3[62]));
    v.set_bit(63, vl::rd(testdouts_3[63]));
    v.set_bit(64, vl::rd(testdouts_3[64]));
    v.set_bit(65, vl::rd(testdouts_3[65]));
    v.set_bit(66, vl::rd(testdouts_3[66]));
    v.set_bit(67, vl::rd(testdouts_3[67]));
    v.set_bit(68, vl::rd(testdouts_3[68]));
    v.set_bit(69, vl::rd(testdouts_3[69]));
    v.set_bit(70, vl::rd(testdouts_3[70]));
    v.set_bit(71, vl::rd(testdouts_3[71]));
    v.set_bit(72, vl::rd(testdouts_3[72]));
    v.set_bit(73, vl::rd(testdouts_3[73]));
    v.set_bit(74, vl::rd(testdouts_3[74]));
    v.set_bit(75, vl::rd(testdouts_3[75]));
    v.set_bit(76, vl::rd(testdouts_3[76]));
    v.set_bit(77, vl::rd(testdouts_3[77]));
    v.set_bit(78, vl::rd(testdouts_3[78]));
    v.set_bit(79, vl::rd(testdouts_3[79]));
    v.set_bit(80, vl::rd(testdouts_3[80]));
    v.set_bit(81, vl::rd(testdouts_3[81]));
    v.set_bit(82, vl::rd(testdouts_3[82]));
    v.set_bit(83, vl::rd(testdouts_3[83]));
    v.set_bit(84, vl::rd(testdouts_3[84]));
    v.set_bit(85, vl::rd(testdouts_3[85]));
    v.set_bit(86, vl::rd(testdouts_3[86]));
    v.set_bit(87, vl::rd(testdouts_3[87]));
    v.set_bit(88, vl::rd(testdouts_3[88]));
    v.set_bit(89, vl::rd(testdouts_3[89]));
    v.set_bit(90, vl::rd(testdouts_3[90]));
    v.set_bit(91, vl::rd(testdouts_3[91]));
    v.set_bit(92, vl::rd(testdouts_3[92]));
    v.set_bit(93, vl::rd(testdouts_3[93]));
    v.set_bit(94, vl::rd(testdouts_3[94]));
    v.set_bit(95, vl::rd(testdouts_3[95]));
    v.set_bit(96, vl::rd(testdouts_3[96]));
    v.set_bit(97, vl::rd(testdouts_3[97]));
    v.set_bit(98, vl::rd(testdouts_3[98]));
    v.set_bit(99, vl::rd(testdouts_3[99]));
    v.set_bit(100, vl::rd(testdouts_3[100]));
    v.set_bit(101, vl::rd(testdouts_3[101]));
    v.set_bit(102, vl::rd(testdouts_3[102]));
    v.set_bit(103, vl::rd(testdouts_3[103]));
    v.set_bit(104, vl::rd(testdouts_3[104]));
    v.set_bit(105, vl::rd(testdouts_3[105]));
    v.set_bit(106, vl::rd(testdouts_3[106]));
    v.set_bit(107, vl::rd(testdouts_3[107]));
    v.set_bit(108, vl::rd(testdouts_3[108]));
    v.set_bit(109, vl::rd(testdouts_3[109]));
    v.set_bit(110, vl::rd(testdouts_3[110]));
    v.set_bit(111, vl::rd(testdouts_3[111]));
    v.set_bit(112, vl::rd(testdouts_3[112]));
    v.set_bit(113, vl::rd(testdouts_3[113]));
    v.set_bit(114, vl::rd(testdouts_3[114]));
    v.set_bit(115, vl::rd(testdouts_3[115]));
    v.set_bit(116, vl::rd(testdouts_3[116]));
    v.set_bit(117, vl::rd(testdouts_3[117]));
    v.set_bit(118, vl::rd(testdouts_3[118]));
    v.set_bit(119, vl::rd(testdouts_3[119]));
    v.set_bit(120, vl::rd(testdouts_3[120]));
    v.set_bit(121, vl::rd(testdouts_3[121]));
    v.set_bit(122, vl::rd(testdouts_3[122]));
    v.set_bit(123, vl::rd(testdouts_3[123]));
    v.set_bit(124, vl::rd(testdouts_3[124]));
    v.set_bit(125, vl::rd(testdouts_3[125]));
    v.set_bit(126, vl::rd(testdouts_3[126]));
    v.set_bit(127, vl::rd(testdouts_3[127]));
    v.set_bit(128, vl::rd(testdouts_3[128]));
    v.set_bit(129, vl::rd(testdouts_3[129]));
    v.set_bit(130, vl::rd(testdouts_3[130]));
    v.set_bit(131, vl::rd(testdouts_3[131]));
    v.set_bit(132, vl::rd(testdouts_3[132]));
    v.set_bit(133, vl::rd(testdouts_3[133]));
    v.set_bit(134, vl::rd(testdouts_3[134]));
    v.set_bit(135, vl::rd(testdouts_3[135]));
    v.set_bit(136, vl::rd(testdouts_3[136]));
    v.set_bit(137, vl::rd(testdouts_3[137]));
    v.set_bit(138, vl::rd(testdouts_3[138]));
    v.set_bit(139, vl::rd(testdouts_3[139]));
    v.set_bit(140, vl::rd(testdouts_3[140]));
    v.set_bit(141, vl::rd(testdouts_3[141]));
    v.set_bit(142, vl::rd(testdouts_3[142]));
    v.set_bit(143, vl::rd(testdouts_3[143]));
    v.set_bit(144, vl::rd(testdouts_3[144]));
    v.set_bit(145, vl::rd(testdouts_3[145]));
    v.set_bit(146, vl::rd(testdouts_3[146]));
    v.set_bit(147, vl::rd(testdouts_3[147]));
    v.set_bit(148, vl::rd(testdouts_3[148]));
    v.set_bit(149, vl::rd(testdouts_3[149]));
    v.set_bit(150, vl::rd(testdouts_3[150]));
    v.set_bit(151, vl::rd(testdouts_3[151]));
    v.set_bit(152, vl::rd(testdouts_3[152]));
    v.set_bit(153, vl::rd(testdouts_3[153]));
    v.set_bit(154, vl::rd(testdouts_3[154]));
    v.set_bit(155, vl::rd(testdouts_3[155]));
    v.set_bit(156, vl::rd(testdouts_3[156]));
    v.set_bit(157, vl::rd(testdouts_3[157]));
    v.set_bit(158, vl::rd(testdouts_3[158]));
    v.set_bit(159, vl::rd(testdouts_3[159]));
    v.set_bit(160, vl::rd(testdouts_3[160]));
    v.set_bit(161, vl::rd(testdouts_3[161]));
    v.set_bit(162, vl::rd(testdouts_3[162]));
    v.set_bit(163, vl::rd(testdouts_3[163]));
    v.set_bit(164, vl::rd(testdouts_3[164]));
    v.set_bit(165, vl::rd(testdouts_3[165]));
    v.set_bit(166, vl::rd(testdouts_3[166]));
    v.set_bit(167, vl::rd(testdouts_3[167]));
    v.set_bit(168, vl::rd(testdouts_3[168]));
    v.set_bit(169, vl::rd(testdouts_3[169]));
    v.set_bit(170, vl::rd(testdouts_3[170]));
    v.set_bit(171, vl::rd(testdouts_3[171]));
    v.set_bit(172, vl::rd(testdouts_3[172]));
    v.set_bit(173, vl::rd(testdouts_3[173]));
    v.set_bit(174, vl::rd(testdouts_3[174]));
    v.set_bit(175, vl::rd(testdouts_3[175]));
    v.set_bit(176, vl::rd(testdouts_3[176]));
    v.set_bit(177, vl::rd(testdouts_3[177]));
    v.set_bit(178, vl::rd(testdouts_3[178]));
    v.set_bit(179, vl::rd(testdouts_3[179]));
    v.set_bit(180, vl::rd(testdouts_3[180]));
    v.set_bit(181, vl::rd(testdouts_3[181]));
    v.set_bit(182, vl::rd(testdouts_3[182]));
    v.set_bit(183, vl::rd(testdouts_3[183]));
    v.set_bit(184, vl::rd(testdouts_3[184]));
    v.set_bit(185, vl::rd(testdouts_3[185]));
    v.set_bit(186, vl::rd(testdouts_3[186]));
    v.set_bit(187, vl::rd(testdouts_3[187]));
    v.set_bit(188, vl::rd(testdouts_3[188]));
    v.set_bit(189, vl::rd(testdouts_3[189]));
    v.set_bit(190, vl::rd(testdouts_3[190]));
    v.set_bit(191, vl::rd(testdouts_3[191]));
    v.set_bit(192, vl::rd(testdouts_3[192]));
    v.set_bit(193, vl::rd(testdouts_3[193]));
    v.set_bit(194, vl::rd(testdouts_3[194]));
    v.set_bit(195, vl::rd(testdouts_3[195]));
    v.set_bit(196, vl::rd(testdouts_3[196]));
    v.set_bit(197, vl::rd(testdouts_3[197]));
    v.set_bit(198, vl::rd(testdouts_3[198]));
    v.set_bit(199, vl::rd(testdouts_3[199]));
    v.set_bit(200, vl::rd(testdouts_3[200]));
    v.set_bit(201, vl::rd(testdouts_3[201]));
    v.set_bit(202, vl::rd(testdouts_3[202]));
    v.set_bit(203, vl::rd(testdouts_3[203]));
    v.set_bit(204, vl::rd(testdouts_3[204]));
    v.set_bit(205, vl::rd(testdouts_3[205]));
    v.set_bit(206, vl::rd(testdouts_3[206]));
    v.set_bit(207, vl::rd(testdouts_3[207]));
    v.set_bit(208, vl::rd(testdouts_3[208]));
    v.set_bit(209, vl::rd(testdouts_3[209]));
    v.set_bit(210, vl::rd(testdouts_3[210]));
    v.set_bit(211, vl::rd(testdouts_3[211]));
    v.set_bit(212, vl::rd(testdouts_3[212]));
    v.set_bit(213, vl::rd(testdouts_3[213]));
    v.set_bit(214, vl::rd(testdouts_3[214]));
    v.set_bit(215, vl::rd(testdouts_3[215]));
    v.set_bit(216, vl::rd(testdouts_3[216]));
    v.set_bit(217, vl::rd(testdouts_3[217]));
    v.set_bit(218, vl::rd(testdouts_3[218]));
    v.set_bit(219, vl::rd(testdouts_3[219]));
    v.set_bit(220, vl::rd(testdouts_3[220]));
    v.set_bit(221, vl::rd(testdouts_3[221]));
    v.set_bit(222, vl::rd(testdouts_3[222]));
    v.set_bit(223, vl::rd(testdouts_3[223]));
    v.set_bit(224, vl::rd(testdouts_3[224]));
    v.set_bit(225, vl::rd(testdouts_3[225]));
    v.set_bit(226, vl::rd(testdouts_3[226]));
    v.set_bit(227, vl::rd(testdouts_3[227]));
    v.set_bit(228, vl::rd(testdouts_3[228]));
    v.set_bit(229, vl::rd(testdouts_3[229]));
    v.set_bit(230, vl::rd(testdouts_3[230]));
    v.set_bit(231, vl::rd(testdouts_3[231]));
    v.set_bit(232, vl::rd(testdouts_3[232]));
    v.set_bit(233, vl::rd(testdouts_3[233]));
    v.set_bit(234, vl::rd(testdouts_3[234]));
    v.set_bit(235, vl::rd(testdouts_3[235]));
    v.set_bit(236, vl::rd(testdouts_3[236]));
    v.set_bit(237, vl::rd(testdouts_3[237]));
    v.set_bit(238, vl::rd(testdouts_3[238]));
    v.set_bit(239, vl::rd(testdouts_3[239]));
    v.set_bit(240, vl::rd(testdouts_3[240]));
    v.set_bit(241, vl::rd(testdouts_3[241]));
    v.set_bit(242, vl::rd(testdouts_3[242]));
    v.set_bit(243, vl::rd(testdouts_3[243]));
    v.set_bit(244, vl::rd(testdouts_3[244]));
    v.set_bit(245, vl::rd(testdouts_3[245]));
    v.set_bit(246, vl::rd(testdouts_3[246]));
    v.set_bit(247, vl::rd(testdouts_3[247]));
    v.set_bit(248, vl::rd(testdouts_3[248]));
    v.set_bit(249, vl::rd(testdouts_3[249]));
    v.set_bit(250, vl::rd(testdouts_3[250]));
    v.set_bit(251, vl::rd(testdouts_3[251]));
    v.set_bit(252, vl::rd(testdouts_3[252]));
    v.set_bit(253, vl::rd(testdouts_3[253]));
    v.set_bit(254, vl::rd(testdouts_3[254]));
    v.set_bit(255, vl::rd(testdouts_3[255]));
    v.set_bit(256, vl::rd(testdouts_3[256]));
    v.set_bit(257, vl::rd(testdouts_3[257]));
    v.set_bit(258, vl::rd(testdouts_3[258]));
    v.set_bit(259, vl::rd(testdouts_3[259]));
    v.set_bit(260, vl::rd(testdouts_3[260]));
    v.set_bit(261, vl::rd(testdouts_3[261]));
    v.set_bit(262, vl::rd(testdouts_3[262]));
    v.set_bit(263, vl::rd(testdouts_3[263]));
    v.set_bit(264, vl::rd(testdouts_3[264]));
    v.set_bit(265, vl::rd(testdouts_3[265]));
    v.set_bit(266, vl::rd(testdouts_3[266]));
    v.set_bit(267, vl::rd(testdouts_3[267]));
    v.set_bit(268, vl::rd(testdouts_3[268]));
    v.set_bit(269, vl::rd(testdouts_3[269]));
    v.set_bit(270, vl::rd(testdouts_3[270]));
    v.set_bit(271, vl::rd(testdouts_3[271]));
    v.set_bit(272, vl::rd(testdouts_3[272]));
    v.set_bit(273, vl::rd(testdouts_3[273]));
    v.set_bit(274, vl::rd(testdouts_3[274]));
    v.set_bit(275, vl::rd(testdouts_3[275]));
    v.set_bit(276, vl::rd(testdouts_3[276]));
    v.set_bit(277, vl::rd(testdouts_3[277]));
    v.set_bit(278, vl::rd(testdouts_3[278]));
    v.set_bit(279, vl::rd(testdouts_3[279]));
    v.set_bit(280, vl::rd(testdouts_3[280]));
    v.set_bit(281, vl::rd(testdouts_3[281]));
    v.set_bit(282, vl::rd(testdouts_3[282]));
    v.set_bit(283, vl::rd(testdouts_3[283]));
    v.set_bit(284, vl::rd(testdouts_3[284]));
    v.set_bit(285, vl::rd(testdouts_3[285]));
    v.set_bit(286, vl::rd(testdouts_3[286]));
    v.set_bit(287, vl::rd(testdouts_3[287]));
    v.set_bit(288, vl::rd(testdouts_3[288]));
    v.set_bit(289, vl::rd(testdouts_3[289]));
    v.set_bit(290, vl::rd(testdouts_3[290]));
    v.set_bit(291, vl::rd(testdouts_3[291]));
    v.set_bit(292, vl::rd(testdouts_3[292]));
    v.set_bit(293, vl::rd(testdouts_3[293]));
    v.set_bit(294, vl::rd(testdouts_3[294]));
    v.set_bit(295, vl::rd(testdouts_3[295]));
    v.set_bit(296, vl::rd(testdouts_3[296]));
    v.set_bit(297, vl::rd(testdouts_3[297]));
    v.set_bit(298, vl::rd(testdouts_3[298]));
    v.set_bit(299, vl::rd(testdouts_3[299]));
    v.set_bit(300, vl::rd(testdouts_3[300]));
    v.set_bit(301, vl::rd(testdouts_3[301]));
    v.set_bit(302, vl::rd(testdouts_3[302]));
    v.set_bit(303, vl::rd(testdouts_3[303]));
    v.set_bit(304, vl::rd(testdouts_3[304]));
    v.set_bit(305, vl::rd(testdouts_3[305]));
    v.set_bit(306, vl::rd(testdouts_3[306]));
    v.set_bit(307, vl::rd(testdouts_3[307]));
    v.set_bit(308, vl::rd(testdouts_3[308]));
    v.set_bit(309, vl::rd(testdouts_3[309]));
    v.set_bit(310, vl::rd(testdouts_3[310]));
    v.set_bit(311, vl::rd(testdouts_3[311]));
    v.set_bit(312, vl::rd(testdouts_3[312]));
    v.set_bit(313, vl::rd(testdouts_3[313]));
    v.set_bit(314, vl::rd(testdouts_3[314]));
    return v;
}

vl::logic<315> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdouts_4() const {
    vl::logic<315> v(0);
    v.set_bit(0, vl::rd(testdouts_4[0]));
    v.set_bit(1, vl::rd(testdouts_4[1]));
    v.set_bit(2, vl::rd(testdouts_4[2]));
    v.set_bit(3, vl::rd(testdouts_4[3]));
    v.set_bit(4, vl::rd(testdouts_4[4]));
    v.set_bit(5, vl::rd(testdouts_4[5]));
    v.set_bit(6, vl::rd(testdouts_4[6]));
    v.set_bit(7, vl::rd(testdouts_4[7]));
    v.set_bit(8, vl::rd(testdouts_4[8]));
    v.set_bit(9, vl::rd(testdouts_4[9]));
    v.set_bit(10, vl::rd(testdouts_4[10]));
    v.set_bit(11, vl::rd(testdouts_4[11]));
    v.set_bit(12, vl::rd(testdouts_4[12]));
    v.set_bit(13, vl::rd(testdouts_4[13]));
    v.set_bit(14, vl::rd(testdouts_4[14]));
    v.set_bit(15, vl::rd(testdouts_4[15]));
    v.set_bit(16, vl::rd(testdouts_4[16]));
    v.set_bit(17, vl::rd(testdouts_4[17]));
    v.set_bit(18, vl::rd(testdouts_4[18]));
    v.set_bit(19, vl::rd(testdouts_4[19]));
    v.set_bit(20, vl::rd(testdouts_4[20]));
    v.set_bit(21, vl::rd(testdouts_4[21]));
    v.set_bit(22, vl::rd(testdouts_4[22]));
    v.set_bit(23, vl::rd(testdouts_4[23]));
    v.set_bit(24, vl::rd(testdouts_4[24]));
    v.set_bit(25, vl::rd(testdouts_4[25]));
    v.set_bit(26, vl::rd(testdouts_4[26]));
    v.set_bit(27, vl::rd(testdouts_4[27]));
    v.set_bit(28, vl::rd(testdouts_4[28]));
    v.set_bit(29, vl::rd(testdouts_4[29]));
    v.set_bit(30, vl::rd(testdouts_4[30]));
    v.set_bit(31, vl::rd(testdouts_4[31]));
    v.set_bit(32, vl::rd(testdouts_4[32]));
    v.set_bit(33, vl::rd(testdouts_4[33]));
    v.set_bit(34, vl::rd(testdouts_4[34]));
    v.set_bit(35, vl::rd(testdouts_4[35]));
    v.set_bit(36, vl::rd(testdouts_4[36]));
    v.set_bit(37, vl::rd(testdouts_4[37]));
    v.set_bit(38, vl::rd(testdouts_4[38]));
    v.set_bit(39, vl::rd(testdouts_4[39]));
    v.set_bit(40, vl::rd(testdouts_4[40]));
    v.set_bit(41, vl::rd(testdouts_4[41]));
    v.set_bit(42, vl::rd(testdouts_4[42]));
    v.set_bit(43, vl::rd(testdouts_4[43]));
    v.set_bit(44, vl::rd(testdouts_4[44]));
    v.set_bit(45, vl::rd(testdouts_4[45]));
    v.set_bit(46, vl::rd(testdouts_4[46]));
    v.set_bit(47, vl::rd(testdouts_4[47]));
    v.set_bit(48, vl::rd(testdouts_4[48]));
    v.set_bit(49, vl::rd(testdouts_4[49]));
    v.set_bit(50, vl::rd(testdouts_4[50]));
    v.set_bit(51, vl::rd(testdouts_4[51]));
    v.set_bit(52, vl::rd(testdouts_4[52]));
    v.set_bit(53, vl::rd(testdouts_4[53]));
    v.set_bit(54, vl::rd(testdouts_4[54]));
    v.set_bit(55, vl::rd(testdouts_4[55]));
    v.set_bit(56, vl::rd(testdouts_4[56]));
    v.set_bit(57, vl::rd(testdouts_4[57]));
    v.set_bit(58, vl::rd(testdouts_4[58]));
    v.set_bit(59, vl::rd(testdouts_4[59]));
    v.set_bit(60, vl::rd(testdouts_4[60]));
    v.set_bit(61, vl::rd(testdouts_4[61]));
    v.set_bit(62, vl::rd(testdouts_4[62]));
    v.set_bit(63, vl::rd(testdouts_4[63]));
    v.set_bit(64, vl::rd(testdouts_4[64]));
    v.set_bit(65, vl::rd(testdouts_4[65]));
    v.set_bit(66, vl::rd(testdouts_4[66]));
    v.set_bit(67, vl::rd(testdouts_4[67]));
    v.set_bit(68, vl::rd(testdouts_4[68]));
    v.set_bit(69, vl::rd(testdouts_4[69]));
    v.set_bit(70, vl::rd(testdouts_4[70]));
    v.set_bit(71, vl::rd(testdouts_4[71]));
    v.set_bit(72, vl::rd(testdouts_4[72]));
    v.set_bit(73, vl::rd(testdouts_4[73]));
    v.set_bit(74, vl::rd(testdouts_4[74]));
    v.set_bit(75, vl::rd(testdouts_4[75]));
    v.set_bit(76, vl::rd(testdouts_4[76]));
    v.set_bit(77, vl::rd(testdouts_4[77]));
    v.set_bit(78, vl::rd(testdouts_4[78]));
    v.set_bit(79, vl::rd(testdouts_4[79]));
    v.set_bit(80, vl::rd(testdouts_4[80]));
    v.set_bit(81, vl::rd(testdouts_4[81]));
    v.set_bit(82, vl::rd(testdouts_4[82]));
    v.set_bit(83, vl::rd(testdouts_4[83]));
    v.set_bit(84, vl::rd(testdouts_4[84]));
    v.set_bit(85, vl::rd(testdouts_4[85]));
    v.set_bit(86, vl::rd(testdouts_4[86]));
    v.set_bit(87, vl::rd(testdouts_4[87]));
    v.set_bit(88, vl::rd(testdouts_4[88]));
    v.set_bit(89, vl::rd(testdouts_4[89]));
    v.set_bit(90, vl::rd(testdouts_4[90]));
    v.set_bit(91, vl::rd(testdouts_4[91]));
    v.set_bit(92, vl::rd(testdouts_4[92]));
    v.set_bit(93, vl::rd(testdouts_4[93]));
    v.set_bit(94, vl::rd(testdouts_4[94]));
    v.set_bit(95, vl::rd(testdouts_4[95]));
    v.set_bit(96, vl::rd(testdouts_4[96]));
    v.set_bit(97, vl::rd(testdouts_4[97]));
    v.set_bit(98, vl::rd(testdouts_4[98]));
    v.set_bit(99, vl::rd(testdouts_4[99]));
    v.set_bit(100, vl::rd(testdouts_4[100]));
    v.set_bit(101, vl::rd(testdouts_4[101]));
    v.set_bit(102, vl::rd(testdouts_4[102]));
    v.set_bit(103, vl::rd(testdouts_4[103]));
    v.set_bit(104, vl::rd(testdouts_4[104]));
    v.set_bit(105, vl::rd(testdouts_4[105]));
    v.set_bit(106, vl::rd(testdouts_4[106]));
    v.set_bit(107, vl::rd(testdouts_4[107]));
    v.set_bit(108, vl::rd(testdouts_4[108]));
    v.set_bit(109, vl::rd(testdouts_4[109]));
    v.set_bit(110, vl::rd(testdouts_4[110]));
    v.set_bit(111, vl::rd(testdouts_4[111]));
    v.set_bit(112, vl::rd(testdouts_4[112]));
    v.set_bit(113, vl::rd(testdouts_4[113]));
    v.set_bit(114, vl::rd(testdouts_4[114]));
    v.set_bit(115, vl::rd(testdouts_4[115]));
    v.set_bit(116, vl::rd(testdouts_4[116]));
    v.set_bit(117, vl::rd(testdouts_4[117]));
    v.set_bit(118, vl::rd(testdouts_4[118]));
    v.set_bit(119, vl::rd(testdouts_4[119]));
    v.set_bit(120, vl::rd(testdouts_4[120]));
    v.set_bit(121, vl::rd(testdouts_4[121]));
    v.set_bit(122, vl::rd(testdouts_4[122]));
    v.set_bit(123, vl::rd(testdouts_4[123]));
    v.set_bit(124, vl::rd(testdouts_4[124]));
    v.set_bit(125, vl::rd(testdouts_4[125]));
    v.set_bit(126, vl::rd(testdouts_4[126]));
    v.set_bit(127, vl::rd(testdouts_4[127]));
    v.set_bit(128, vl::rd(testdouts_4[128]));
    v.set_bit(129, vl::rd(testdouts_4[129]));
    v.set_bit(130, vl::rd(testdouts_4[130]));
    v.set_bit(131, vl::rd(testdouts_4[131]));
    v.set_bit(132, vl::rd(testdouts_4[132]));
    v.set_bit(133, vl::rd(testdouts_4[133]));
    v.set_bit(134, vl::rd(testdouts_4[134]));
    v.set_bit(135, vl::rd(testdouts_4[135]));
    v.set_bit(136, vl::rd(testdouts_4[136]));
    v.set_bit(137, vl::rd(testdouts_4[137]));
    v.set_bit(138, vl::rd(testdouts_4[138]));
    v.set_bit(139, vl::rd(testdouts_4[139]));
    v.set_bit(140, vl::rd(testdouts_4[140]));
    v.set_bit(141, vl::rd(testdouts_4[141]));
    v.set_bit(142, vl::rd(testdouts_4[142]));
    v.set_bit(143, vl::rd(testdouts_4[143]));
    v.set_bit(144, vl::rd(testdouts_4[144]));
    v.set_bit(145, vl::rd(testdouts_4[145]));
    v.set_bit(146, vl::rd(testdouts_4[146]));
    v.set_bit(147, vl::rd(testdouts_4[147]));
    v.set_bit(148, vl::rd(testdouts_4[148]));
    v.set_bit(149, vl::rd(testdouts_4[149]));
    v.set_bit(150, vl::rd(testdouts_4[150]));
    v.set_bit(151, vl::rd(testdouts_4[151]));
    v.set_bit(152, vl::rd(testdouts_4[152]));
    v.set_bit(153, vl::rd(testdouts_4[153]));
    v.set_bit(154, vl::rd(testdouts_4[154]));
    v.set_bit(155, vl::rd(testdouts_4[155]));
    v.set_bit(156, vl::rd(testdouts_4[156]));
    v.set_bit(157, vl::rd(testdouts_4[157]));
    v.set_bit(158, vl::rd(testdouts_4[158]));
    v.set_bit(159, vl::rd(testdouts_4[159]));
    v.set_bit(160, vl::rd(testdouts_4[160]));
    v.set_bit(161, vl::rd(testdouts_4[161]));
    v.set_bit(162, vl::rd(testdouts_4[162]));
    v.set_bit(163, vl::rd(testdouts_4[163]));
    v.set_bit(164, vl::rd(testdouts_4[164]));
    v.set_bit(165, vl::rd(testdouts_4[165]));
    v.set_bit(166, vl::rd(testdouts_4[166]));
    v.set_bit(167, vl::rd(testdouts_4[167]));
    v.set_bit(168, vl::rd(testdouts_4[168]));
    v.set_bit(169, vl::rd(testdouts_4[169]));
    v.set_bit(170, vl::rd(testdouts_4[170]));
    v.set_bit(171, vl::rd(testdouts_4[171]));
    v.set_bit(172, vl::rd(testdouts_4[172]));
    v.set_bit(173, vl::rd(testdouts_4[173]));
    v.set_bit(174, vl::rd(testdouts_4[174]));
    v.set_bit(175, vl::rd(testdouts_4[175]));
    v.set_bit(176, vl::rd(testdouts_4[176]));
    v.set_bit(177, vl::rd(testdouts_4[177]));
    v.set_bit(178, vl::rd(testdouts_4[178]));
    v.set_bit(179, vl::rd(testdouts_4[179]));
    v.set_bit(180, vl::rd(testdouts_4[180]));
    v.set_bit(181, vl::rd(testdouts_4[181]));
    v.set_bit(182, vl::rd(testdouts_4[182]));
    v.set_bit(183, vl::rd(testdouts_4[183]));
    v.set_bit(184, vl::rd(testdouts_4[184]));
    v.set_bit(185, vl::rd(testdouts_4[185]));
    v.set_bit(186, vl::rd(testdouts_4[186]));
    v.set_bit(187, vl::rd(testdouts_4[187]));
    v.set_bit(188, vl::rd(testdouts_4[188]));
    v.set_bit(189, vl::rd(testdouts_4[189]));
    v.set_bit(190, vl::rd(testdouts_4[190]));
    v.set_bit(191, vl::rd(testdouts_4[191]));
    v.set_bit(192, vl::rd(testdouts_4[192]));
    v.set_bit(193, vl::rd(testdouts_4[193]));
    v.set_bit(194, vl::rd(testdouts_4[194]));
    v.set_bit(195, vl::rd(testdouts_4[195]));
    v.set_bit(196, vl::rd(testdouts_4[196]));
    v.set_bit(197, vl::rd(testdouts_4[197]));
    v.set_bit(198, vl::rd(testdouts_4[198]));
    v.set_bit(199, vl::rd(testdouts_4[199]));
    v.set_bit(200, vl::rd(testdouts_4[200]));
    v.set_bit(201, vl::rd(testdouts_4[201]));
    v.set_bit(202, vl::rd(testdouts_4[202]));
    v.set_bit(203, vl::rd(testdouts_4[203]));
    v.set_bit(204, vl::rd(testdouts_4[204]));
    v.set_bit(205, vl::rd(testdouts_4[205]));
    v.set_bit(206, vl::rd(testdouts_4[206]));
    v.set_bit(207, vl::rd(testdouts_4[207]));
    v.set_bit(208, vl::rd(testdouts_4[208]));
    v.set_bit(209, vl::rd(testdouts_4[209]));
    v.set_bit(210, vl::rd(testdouts_4[210]));
    v.set_bit(211, vl::rd(testdouts_4[211]));
    v.set_bit(212, vl::rd(testdouts_4[212]));
    v.set_bit(213, vl::rd(testdouts_4[213]));
    v.set_bit(214, vl::rd(testdouts_4[214]));
    v.set_bit(215, vl::rd(testdouts_4[215]));
    v.set_bit(216, vl::rd(testdouts_4[216]));
    v.set_bit(217, vl::rd(testdouts_4[217]));
    v.set_bit(218, vl::rd(testdouts_4[218]));
    v.set_bit(219, vl::rd(testdouts_4[219]));
    v.set_bit(220, vl::rd(testdouts_4[220]));
    v.set_bit(221, vl::rd(testdouts_4[221]));
    v.set_bit(222, vl::rd(testdouts_4[222]));
    v.set_bit(223, vl::rd(testdouts_4[223]));
    v.set_bit(224, vl::rd(testdouts_4[224]));
    v.set_bit(225, vl::rd(testdouts_4[225]));
    v.set_bit(226, vl::rd(testdouts_4[226]));
    v.set_bit(227, vl::rd(testdouts_4[227]));
    v.set_bit(228, vl::rd(testdouts_4[228]));
    v.set_bit(229, vl::rd(testdouts_4[229]));
    v.set_bit(230, vl::rd(testdouts_4[230]));
    v.set_bit(231, vl::rd(testdouts_4[231]));
    v.set_bit(232, vl::rd(testdouts_4[232]));
    v.set_bit(233, vl::rd(testdouts_4[233]));
    v.set_bit(234, vl::rd(testdouts_4[234]));
    v.set_bit(235, vl::rd(testdouts_4[235]));
    v.set_bit(236, vl::rd(testdouts_4[236]));
    v.set_bit(237, vl::rd(testdouts_4[237]));
    v.set_bit(238, vl::rd(testdouts_4[238]));
    v.set_bit(239, vl::rd(testdouts_4[239]));
    v.set_bit(240, vl::rd(testdouts_4[240]));
    v.set_bit(241, vl::rd(testdouts_4[241]));
    v.set_bit(242, vl::rd(testdouts_4[242]));
    v.set_bit(243, vl::rd(testdouts_4[243]));
    v.set_bit(244, vl::rd(testdouts_4[244]));
    v.set_bit(245, vl::rd(testdouts_4[245]));
    v.set_bit(246, vl::rd(testdouts_4[246]));
    v.set_bit(247, vl::rd(testdouts_4[247]));
    v.set_bit(248, vl::rd(testdouts_4[248]));
    v.set_bit(249, vl::rd(testdouts_4[249]));
    v.set_bit(250, vl::rd(testdouts_4[250]));
    v.set_bit(251, vl::rd(testdouts_4[251]));
    v.set_bit(252, vl::rd(testdouts_4[252]));
    v.set_bit(253, vl::rd(testdouts_4[253]));
    v.set_bit(254, vl::rd(testdouts_4[254]));
    v.set_bit(255, vl::rd(testdouts_4[255]));
    v.set_bit(256, vl::rd(testdouts_4[256]));
    v.set_bit(257, vl::rd(testdouts_4[257]));
    v.set_bit(258, vl::rd(testdouts_4[258]));
    v.set_bit(259, vl::rd(testdouts_4[259]));
    v.set_bit(260, vl::rd(testdouts_4[260]));
    v.set_bit(261, vl::rd(testdouts_4[261]));
    v.set_bit(262, vl::rd(testdouts_4[262]));
    v.set_bit(263, vl::rd(testdouts_4[263]));
    v.set_bit(264, vl::rd(testdouts_4[264]));
    v.set_bit(265, vl::rd(testdouts_4[265]));
    v.set_bit(266, vl::rd(testdouts_4[266]));
    v.set_bit(267, vl::rd(testdouts_4[267]));
    v.set_bit(268, vl::rd(testdouts_4[268]));
    v.set_bit(269, vl::rd(testdouts_4[269]));
    v.set_bit(270, vl::rd(testdouts_4[270]));
    v.set_bit(271, vl::rd(testdouts_4[271]));
    v.set_bit(272, vl::rd(testdouts_4[272]));
    v.set_bit(273, vl::rd(testdouts_4[273]));
    v.set_bit(274, vl::rd(testdouts_4[274]));
    v.set_bit(275, vl::rd(testdouts_4[275]));
    v.set_bit(276, vl::rd(testdouts_4[276]));
    v.set_bit(277, vl::rd(testdouts_4[277]));
    v.set_bit(278, vl::rd(testdouts_4[278]));
    v.set_bit(279, vl::rd(testdouts_4[279]));
    v.set_bit(280, vl::rd(testdouts_4[280]));
    v.set_bit(281, vl::rd(testdouts_4[281]));
    v.set_bit(282, vl::rd(testdouts_4[282]));
    v.set_bit(283, vl::rd(testdouts_4[283]));
    v.set_bit(284, vl::rd(testdouts_4[284]));
    v.set_bit(285, vl::rd(testdouts_4[285]));
    v.set_bit(286, vl::rd(testdouts_4[286]));
    v.set_bit(287, vl::rd(testdouts_4[287]));
    v.set_bit(288, vl::rd(testdouts_4[288]));
    v.set_bit(289, vl::rd(testdouts_4[289]));
    v.set_bit(290, vl::rd(testdouts_4[290]));
    v.set_bit(291, vl::rd(testdouts_4[291]));
    v.set_bit(292, vl::rd(testdouts_4[292]));
    v.set_bit(293, vl::rd(testdouts_4[293]));
    v.set_bit(294, vl::rd(testdouts_4[294]));
    v.set_bit(295, vl::rd(testdouts_4[295]));
    v.set_bit(296, vl::rd(testdouts_4[296]));
    v.set_bit(297, vl::rd(testdouts_4[297]));
    v.set_bit(298, vl::rd(testdouts_4[298]));
    v.set_bit(299, vl::rd(testdouts_4[299]));
    v.set_bit(300, vl::rd(testdouts_4[300]));
    v.set_bit(301, vl::rd(testdouts_4[301]));
    v.set_bit(302, vl::rd(testdouts_4[302]));
    v.set_bit(303, vl::rd(testdouts_4[303]));
    v.set_bit(304, vl::rd(testdouts_4[304]));
    v.set_bit(305, vl::rd(testdouts_4[305]));
    v.set_bit(306, vl::rd(testdouts_4[306]));
    v.set_bit(307, vl::rd(testdouts_4[307]));
    v.set_bit(308, vl::rd(testdouts_4[308]));
    v.set_bit(309, vl::rd(testdouts_4[309]));
    v.set_bit(310, vl::rd(testdouts_4[310]));
    v.set_bit(311, vl::rd(testdouts_4[311]));
    v.set_bit(312, vl::rd(testdouts_4[312]));
    v.set_bit(313, vl::rd(testdouts_4[313]));
    v.set_bit(314, vl::rd(testdouts_4[314]));
    return v;
}

vl::logic<315> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdouts_5() const {
    vl::logic<315> v(0);
    v.set_bit(0, vl::rd(testdouts_5[0]));
    v.set_bit(1, vl::rd(testdouts_5[1]));
    v.set_bit(2, vl::rd(testdouts_5[2]));
    v.set_bit(3, vl::rd(testdouts_5[3]));
    v.set_bit(4, vl::rd(testdouts_5[4]));
    v.set_bit(5, vl::rd(testdouts_5[5]));
    v.set_bit(6, vl::rd(testdouts_5[6]));
    v.set_bit(7, vl::rd(testdouts_5[7]));
    v.set_bit(8, vl::rd(testdouts_5[8]));
    v.set_bit(9, vl::rd(testdouts_5[9]));
    v.set_bit(10, vl::rd(testdouts_5[10]));
    v.set_bit(11, vl::rd(testdouts_5[11]));
    v.set_bit(12, vl::rd(testdouts_5[12]));
    v.set_bit(13, vl::rd(testdouts_5[13]));
    v.set_bit(14, vl::rd(testdouts_5[14]));
    v.set_bit(15, vl::rd(testdouts_5[15]));
    v.set_bit(16, vl::rd(testdouts_5[16]));
    v.set_bit(17, vl::rd(testdouts_5[17]));
    v.set_bit(18, vl::rd(testdouts_5[18]));
    v.set_bit(19, vl::rd(testdouts_5[19]));
    v.set_bit(20, vl::rd(testdouts_5[20]));
    v.set_bit(21, vl::rd(testdouts_5[21]));
    v.set_bit(22, vl::rd(testdouts_5[22]));
    v.set_bit(23, vl::rd(testdouts_5[23]));
    v.set_bit(24, vl::rd(testdouts_5[24]));
    v.set_bit(25, vl::rd(testdouts_5[25]));
    v.set_bit(26, vl::rd(testdouts_5[26]));
    v.set_bit(27, vl::rd(testdouts_5[27]));
    v.set_bit(28, vl::rd(testdouts_5[28]));
    v.set_bit(29, vl::rd(testdouts_5[29]));
    v.set_bit(30, vl::rd(testdouts_5[30]));
    v.set_bit(31, vl::rd(testdouts_5[31]));
    v.set_bit(32, vl::rd(testdouts_5[32]));
    v.set_bit(33, vl::rd(testdouts_5[33]));
    v.set_bit(34, vl::rd(testdouts_5[34]));
    v.set_bit(35, vl::rd(testdouts_5[35]));
    v.set_bit(36, vl::rd(testdouts_5[36]));
    v.set_bit(37, vl::rd(testdouts_5[37]));
    v.set_bit(38, vl::rd(testdouts_5[38]));
    v.set_bit(39, vl::rd(testdouts_5[39]));
    v.set_bit(40, vl::rd(testdouts_5[40]));
    v.set_bit(41, vl::rd(testdouts_5[41]));
    v.set_bit(42, vl::rd(testdouts_5[42]));
    v.set_bit(43, vl::rd(testdouts_5[43]));
    v.set_bit(44, vl::rd(testdouts_5[44]));
    v.set_bit(45, vl::rd(testdouts_5[45]));
    v.set_bit(46, vl::rd(testdouts_5[46]));
    v.set_bit(47, vl::rd(testdouts_5[47]));
    v.set_bit(48, vl::rd(testdouts_5[48]));
    v.set_bit(49, vl::rd(testdouts_5[49]));
    v.set_bit(50, vl::rd(testdouts_5[50]));
    v.set_bit(51, vl::rd(testdouts_5[51]));
    v.set_bit(52, vl::rd(testdouts_5[52]));
    v.set_bit(53, vl::rd(testdouts_5[53]));
    v.set_bit(54, vl::rd(testdouts_5[54]));
    v.set_bit(55, vl::rd(testdouts_5[55]));
    v.set_bit(56, vl::rd(testdouts_5[56]));
    v.set_bit(57, vl::rd(testdouts_5[57]));
    v.set_bit(58, vl::rd(testdouts_5[58]));
    v.set_bit(59, vl::rd(testdouts_5[59]));
    v.set_bit(60, vl::rd(testdouts_5[60]));
    v.set_bit(61, vl::rd(testdouts_5[61]));
    v.set_bit(62, vl::rd(testdouts_5[62]));
    v.set_bit(63, vl::rd(testdouts_5[63]));
    v.set_bit(64, vl::rd(testdouts_5[64]));
    v.set_bit(65, vl::rd(testdouts_5[65]));
    v.set_bit(66, vl::rd(testdouts_5[66]));
    v.set_bit(67, vl::rd(testdouts_5[67]));
    v.set_bit(68, vl::rd(testdouts_5[68]));
    v.set_bit(69, vl::rd(testdouts_5[69]));
    v.set_bit(70, vl::rd(testdouts_5[70]));
    v.set_bit(71, vl::rd(testdouts_5[71]));
    v.set_bit(72, vl::rd(testdouts_5[72]));
    v.set_bit(73, vl::rd(testdouts_5[73]));
    v.set_bit(74, vl::rd(testdouts_5[74]));
    v.set_bit(75, vl::rd(testdouts_5[75]));
    v.set_bit(76, vl::rd(testdouts_5[76]));
    v.set_bit(77, vl::rd(testdouts_5[77]));
    v.set_bit(78, vl::rd(testdouts_5[78]));
    v.set_bit(79, vl::rd(testdouts_5[79]));
    v.set_bit(80, vl::rd(testdouts_5[80]));
    v.set_bit(81, vl::rd(testdouts_5[81]));
    v.set_bit(82, vl::rd(testdouts_5[82]));
    v.set_bit(83, vl::rd(testdouts_5[83]));
    v.set_bit(84, vl::rd(testdouts_5[84]));
    v.set_bit(85, vl::rd(testdouts_5[85]));
    v.set_bit(86, vl::rd(testdouts_5[86]));
    v.set_bit(87, vl::rd(testdouts_5[87]));
    v.set_bit(88, vl::rd(testdouts_5[88]));
    v.set_bit(89, vl::rd(testdouts_5[89]));
    v.set_bit(90, vl::rd(testdouts_5[90]));
    v.set_bit(91, vl::rd(testdouts_5[91]));
    v.set_bit(92, vl::rd(testdouts_5[92]));
    v.set_bit(93, vl::rd(testdouts_5[93]));
    v.set_bit(94, vl::rd(testdouts_5[94]));
    v.set_bit(95, vl::rd(testdouts_5[95]));
    v.set_bit(96, vl::rd(testdouts_5[96]));
    v.set_bit(97, vl::rd(testdouts_5[97]));
    v.set_bit(98, vl::rd(testdouts_5[98]));
    v.set_bit(99, vl::rd(testdouts_5[99]));
    v.set_bit(100, vl::rd(testdouts_5[100]));
    v.set_bit(101, vl::rd(testdouts_5[101]));
    v.set_bit(102, vl::rd(testdouts_5[102]));
    v.set_bit(103, vl::rd(testdouts_5[103]));
    v.set_bit(104, vl::rd(testdouts_5[104]));
    v.set_bit(105, vl::rd(testdouts_5[105]));
    v.set_bit(106, vl::rd(testdouts_5[106]));
    v.set_bit(107, vl::rd(testdouts_5[107]));
    v.set_bit(108, vl::rd(testdouts_5[108]));
    v.set_bit(109, vl::rd(testdouts_5[109]));
    v.set_bit(110, vl::rd(testdouts_5[110]));
    v.set_bit(111, vl::rd(testdouts_5[111]));
    v.set_bit(112, vl::rd(testdouts_5[112]));
    v.set_bit(113, vl::rd(testdouts_5[113]));
    v.set_bit(114, vl::rd(testdouts_5[114]));
    v.set_bit(115, vl::rd(testdouts_5[115]));
    v.set_bit(116, vl::rd(testdouts_5[116]));
    v.set_bit(117, vl::rd(testdouts_5[117]));
    v.set_bit(118, vl::rd(testdouts_5[118]));
    v.set_bit(119, vl::rd(testdouts_5[119]));
    v.set_bit(120, vl::rd(testdouts_5[120]));
    v.set_bit(121, vl::rd(testdouts_5[121]));
    v.set_bit(122, vl::rd(testdouts_5[122]));
    v.set_bit(123, vl::rd(testdouts_5[123]));
    v.set_bit(124, vl::rd(testdouts_5[124]));
    v.set_bit(125, vl::rd(testdouts_5[125]));
    v.set_bit(126, vl::rd(testdouts_5[126]));
    v.set_bit(127, vl::rd(testdouts_5[127]));
    v.set_bit(128, vl::rd(testdouts_5[128]));
    v.set_bit(129, vl::rd(testdouts_5[129]));
    v.set_bit(130, vl::rd(testdouts_5[130]));
    v.set_bit(131, vl::rd(testdouts_5[131]));
    v.set_bit(132, vl::rd(testdouts_5[132]));
    v.set_bit(133, vl::rd(testdouts_5[133]));
    v.set_bit(134, vl::rd(testdouts_5[134]));
    v.set_bit(135, vl::rd(testdouts_5[135]));
    v.set_bit(136, vl::rd(testdouts_5[136]));
    v.set_bit(137, vl::rd(testdouts_5[137]));
    v.set_bit(138, vl::rd(testdouts_5[138]));
    v.set_bit(139, vl::rd(testdouts_5[139]));
    v.set_bit(140, vl::rd(testdouts_5[140]));
    v.set_bit(141, vl::rd(testdouts_5[141]));
    v.set_bit(142, vl::rd(testdouts_5[142]));
    v.set_bit(143, vl::rd(testdouts_5[143]));
    v.set_bit(144, vl::rd(testdouts_5[144]));
    v.set_bit(145, vl::rd(testdouts_5[145]));
    v.set_bit(146, vl::rd(testdouts_5[146]));
    v.set_bit(147, vl::rd(testdouts_5[147]));
    v.set_bit(148, vl::rd(testdouts_5[148]));
    v.set_bit(149, vl::rd(testdouts_5[149]));
    v.set_bit(150, vl::rd(testdouts_5[150]));
    v.set_bit(151, vl::rd(testdouts_5[151]));
    v.set_bit(152, vl::rd(testdouts_5[152]));
    v.set_bit(153, vl::rd(testdouts_5[153]));
    v.set_bit(154, vl::rd(testdouts_5[154]));
    v.set_bit(155, vl::rd(testdouts_5[155]));
    v.set_bit(156, vl::rd(testdouts_5[156]));
    v.set_bit(157, vl::rd(testdouts_5[157]));
    v.set_bit(158, vl::rd(testdouts_5[158]));
    v.set_bit(159, vl::rd(testdouts_5[159]));
    v.set_bit(160, vl::rd(testdouts_5[160]));
    v.set_bit(161, vl::rd(testdouts_5[161]));
    v.set_bit(162, vl::rd(testdouts_5[162]));
    v.set_bit(163, vl::rd(testdouts_5[163]));
    v.set_bit(164, vl::rd(testdouts_5[164]));
    v.set_bit(165, vl::rd(testdouts_5[165]));
    v.set_bit(166, vl::rd(testdouts_5[166]));
    v.set_bit(167, vl::rd(testdouts_5[167]));
    v.set_bit(168, vl::rd(testdouts_5[168]));
    v.set_bit(169, vl::rd(testdouts_5[169]));
    v.set_bit(170, vl::rd(testdouts_5[170]));
    v.set_bit(171, vl::rd(testdouts_5[171]));
    v.set_bit(172, vl::rd(testdouts_5[172]));
    v.set_bit(173, vl::rd(testdouts_5[173]));
    v.set_bit(174, vl::rd(testdouts_5[174]));
    v.set_bit(175, vl::rd(testdouts_5[175]));
    v.set_bit(176, vl::rd(testdouts_5[176]));
    v.set_bit(177, vl::rd(testdouts_5[177]));
    v.set_bit(178, vl::rd(testdouts_5[178]));
    v.set_bit(179, vl::rd(testdouts_5[179]));
    v.set_bit(180, vl::rd(testdouts_5[180]));
    v.set_bit(181, vl::rd(testdouts_5[181]));
    v.set_bit(182, vl::rd(testdouts_5[182]));
    v.set_bit(183, vl::rd(testdouts_5[183]));
    v.set_bit(184, vl::rd(testdouts_5[184]));
    v.set_bit(185, vl::rd(testdouts_5[185]));
    v.set_bit(186, vl::rd(testdouts_5[186]));
    v.set_bit(187, vl::rd(testdouts_5[187]));
    v.set_bit(188, vl::rd(testdouts_5[188]));
    v.set_bit(189, vl::rd(testdouts_5[189]));
    v.set_bit(190, vl::rd(testdouts_5[190]));
    v.set_bit(191, vl::rd(testdouts_5[191]));
    v.set_bit(192, vl::rd(testdouts_5[192]));
    v.set_bit(193, vl::rd(testdouts_5[193]));
    v.set_bit(194, vl::rd(testdouts_5[194]));
    v.set_bit(195, vl::rd(testdouts_5[195]));
    v.set_bit(196, vl::rd(testdouts_5[196]));
    v.set_bit(197, vl::rd(testdouts_5[197]));
    v.set_bit(198, vl::rd(testdouts_5[198]));
    v.set_bit(199, vl::rd(testdouts_5[199]));
    v.set_bit(200, vl::rd(testdouts_5[200]));
    v.set_bit(201, vl::rd(testdouts_5[201]));
    v.set_bit(202, vl::rd(testdouts_5[202]));
    v.set_bit(203, vl::rd(testdouts_5[203]));
    v.set_bit(204, vl::rd(testdouts_5[204]));
    v.set_bit(205, vl::rd(testdouts_5[205]));
    v.set_bit(206, vl::rd(testdouts_5[206]));
    v.set_bit(207, vl::rd(testdouts_5[207]));
    v.set_bit(208, vl::rd(testdouts_5[208]));
    v.set_bit(209, vl::rd(testdouts_5[209]));
    v.set_bit(210, vl::rd(testdouts_5[210]));
    v.set_bit(211, vl::rd(testdouts_5[211]));
    v.set_bit(212, vl::rd(testdouts_5[212]));
    v.set_bit(213, vl::rd(testdouts_5[213]));
    v.set_bit(214, vl::rd(testdouts_5[214]));
    v.set_bit(215, vl::rd(testdouts_5[215]));
    v.set_bit(216, vl::rd(testdouts_5[216]));
    v.set_bit(217, vl::rd(testdouts_5[217]));
    v.set_bit(218, vl::rd(testdouts_5[218]));
    v.set_bit(219, vl::rd(testdouts_5[219]));
    v.set_bit(220, vl::rd(testdouts_5[220]));
    v.set_bit(221, vl::rd(testdouts_5[221]));
    v.set_bit(222, vl::rd(testdouts_5[222]));
    v.set_bit(223, vl::rd(testdouts_5[223]));
    v.set_bit(224, vl::rd(testdouts_5[224]));
    v.set_bit(225, vl::rd(testdouts_5[225]));
    v.set_bit(226, vl::rd(testdouts_5[226]));
    v.set_bit(227, vl::rd(testdouts_5[227]));
    v.set_bit(228, vl::rd(testdouts_5[228]));
    v.set_bit(229, vl::rd(testdouts_5[229]));
    v.set_bit(230, vl::rd(testdouts_5[230]));
    v.set_bit(231, vl::rd(testdouts_5[231]));
    v.set_bit(232, vl::rd(testdouts_5[232]));
    v.set_bit(233, vl::rd(testdouts_5[233]));
    v.set_bit(234, vl::rd(testdouts_5[234]));
    v.set_bit(235, vl::rd(testdouts_5[235]));
    v.set_bit(236, vl::rd(testdouts_5[236]));
    v.set_bit(237, vl::rd(testdouts_5[237]));
    v.set_bit(238, vl::rd(testdouts_5[238]));
    v.set_bit(239, vl::rd(testdouts_5[239]));
    v.set_bit(240, vl::rd(testdouts_5[240]));
    v.set_bit(241, vl::rd(testdouts_5[241]));
    v.set_bit(242, vl::rd(testdouts_5[242]));
    v.set_bit(243, vl::rd(testdouts_5[243]));
    v.set_bit(244, vl::rd(testdouts_5[244]));
    v.set_bit(245, vl::rd(testdouts_5[245]));
    v.set_bit(246, vl::rd(testdouts_5[246]));
    v.set_bit(247, vl::rd(testdouts_5[247]));
    v.set_bit(248, vl::rd(testdouts_5[248]));
    v.set_bit(249, vl::rd(testdouts_5[249]));
    v.set_bit(250, vl::rd(testdouts_5[250]));
    v.set_bit(251, vl::rd(testdouts_5[251]));
    v.set_bit(252, vl::rd(testdouts_5[252]));
    v.set_bit(253, vl::rd(testdouts_5[253]));
    v.set_bit(254, vl::rd(testdouts_5[254]));
    v.set_bit(255, vl::rd(testdouts_5[255]));
    v.set_bit(256, vl::rd(testdouts_5[256]));
    v.set_bit(257, vl::rd(testdouts_5[257]));
    v.set_bit(258, vl::rd(testdouts_5[258]));
    v.set_bit(259, vl::rd(testdouts_5[259]));
    v.set_bit(260, vl::rd(testdouts_5[260]));
    v.set_bit(261, vl::rd(testdouts_5[261]));
    v.set_bit(262, vl::rd(testdouts_5[262]));
    v.set_bit(263, vl::rd(testdouts_5[263]));
    v.set_bit(264, vl::rd(testdouts_5[264]));
    v.set_bit(265, vl::rd(testdouts_5[265]));
    v.set_bit(266, vl::rd(testdouts_5[266]));
    v.set_bit(267, vl::rd(testdouts_5[267]));
    v.set_bit(268, vl::rd(testdouts_5[268]));
    v.set_bit(269, vl::rd(testdouts_5[269]));
    v.set_bit(270, vl::rd(testdouts_5[270]));
    v.set_bit(271, vl::rd(testdouts_5[271]));
    v.set_bit(272, vl::rd(testdouts_5[272]));
    v.set_bit(273, vl::rd(testdouts_5[273]));
    v.set_bit(274, vl::rd(testdouts_5[274]));
    v.set_bit(275, vl::rd(testdouts_5[275]));
    v.set_bit(276, vl::rd(testdouts_5[276]));
    v.set_bit(277, vl::rd(testdouts_5[277]));
    v.set_bit(278, vl::rd(testdouts_5[278]));
    v.set_bit(279, vl::rd(testdouts_5[279]));
    v.set_bit(280, vl::rd(testdouts_5[280]));
    v.set_bit(281, vl::rd(testdouts_5[281]));
    v.set_bit(282, vl::rd(testdouts_5[282]));
    v.set_bit(283, vl::rd(testdouts_5[283]));
    v.set_bit(284, vl::rd(testdouts_5[284]));
    v.set_bit(285, vl::rd(testdouts_5[285]));
    v.set_bit(286, vl::rd(testdouts_5[286]));
    v.set_bit(287, vl::rd(testdouts_5[287]));
    v.set_bit(288, vl::rd(testdouts_5[288]));
    v.set_bit(289, vl::rd(testdouts_5[289]));
    v.set_bit(290, vl::rd(testdouts_5[290]));
    v.set_bit(291, vl::rd(testdouts_5[291]));
    v.set_bit(292, vl::rd(testdouts_5[292]));
    v.set_bit(293, vl::rd(testdouts_5[293]));
    v.set_bit(294, vl::rd(testdouts_5[294]));
    v.set_bit(295, vl::rd(testdouts_5[295]));
    v.set_bit(296, vl::rd(testdouts_5[296]));
    v.set_bit(297, vl::rd(testdouts_5[297]));
    v.set_bit(298, vl::rd(testdouts_5[298]));
    v.set_bit(299, vl::rd(testdouts_5[299]));
    v.set_bit(300, vl::rd(testdouts_5[300]));
    v.set_bit(301, vl::rd(testdouts_5[301]));
    v.set_bit(302, vl::rd(testdouts_5[302]));
    v.set_bit(303, vl::rd(testdouts_5[303]));
    v.set_bit(304, vl::rd(testdouts_5[304]));
    v.set_bit(305, vl::rd(testdouts_5[305]));
    v.set_bit(306, vl::rd(testdouts_5[306]));
    v.set_bit(307, vl::rd(testdouts_5[307]));
    v.set_bit(308, vl::rd(testdouts_5[308]));
    v.set_bit(309, vl::rd(testdouts_5[309]));
    v.set_bit(310, vl::rd(testdouts_5[310]));
    v.set_bit(311, vl::rd(testdouts_5[311]));
    v.set_bit(312, vl::rd(testdouts_5[312]));
    v.set_bit(313, vl::rd(testdouts_5[313]));
    v.set_bit(314, vl::rd(testdouts_5[314]));
    return v;
}

vl::logic<315> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdouts_6() const {
    vl::logic<315> v(0);
    v.set_bit(0, vl::rd(testdouts_6[0]));
    v.set_bit(1, vl::rd(testdouts_6[1]));
    v.set_bit(2, vl::rd(testdouts_6[2]));
    v.set_bit(3, vl::rd(testdouts_6[3]));
    v.set_bit(4, vl::rd(testdouts_6[4]));
    v.set_bit(5, vl::rd(testdouts_6[5]));
    v.set_bit(6, vl::rd(testdouts_6[6]));
    v.set_bit(7, vl::rd(testdouts_6[7]));
    v.set_bit(8, vl::rd(testdouts_6[8]));
    v.set_bit(9, vl::rd(testdouts_6[9]));
    v.set_bit(10, vl::rd(testdouts_6[10]));
    v.set_bit(11, vl::rd(testdouts_6[11]));
    v.set_bit(12, vl::rd(testdouts_6[12]));
    v.set_bit(13, vl::rd(testdouts_6[13]));
    v.set_bit(14, vl::rd(testdouts_6[14]));
    v.set_bit(15, vl::rd(testdouts_6[15]));
    v.set_bit(16, vl::rd(testdouts_6[16]));
    v.set_bit(17, vl::rd(testdouts_6[17]));
    v.set_bit(18, vl::rd(testdouts_6[18]));
    v.set_bit(19, vl::rd(testdouts_6[19]));
    v.set_bit(20, vl::rd(testdouts_6[20]));
    v.set_bit(21, vl::rd(testdouts_6[21]));
    v.set_bit(22, vl::rd(testdouts_6[22]));
    v.set_bit(23, vl::rd(testdouts_6[23]));
    v.set_bit(24, vl::rd(testdouts_6[24]));
    v.set_bit(25, vl::rd(testdouts_6[25]));
    v.set_bit(26, vl::rd(testdouts_6[26]));
    v.set_bit(27, vl::rd(testdouts_6[27]));
    v.set_bit(28, vl::rd(testdouts_6[28]));
    v.set_bit(29, vl::rd(testdouts_6[29]));
    v.set_bit(30, vl::rd(testdouts_6[30]));
    v.set_bit(31, vl::rd(testdouts_6[31]));
    v.set_bit(32, vl::rd(testdouts_6[32]));
    v.set_bit(33, vl::rd(testdouts_6[33]));
    v.set_bit(34, vl::rd(testdouts_6[34]));
    v.set_bit(35, vl::rd(testdouts_6[35]));
    v.set_bit(36, vl::rd(testdouts_6[36]));
    v.set_bit(37, vl::rd(testdouts_6[37]));
    v.set_bit(38, vl::rd(testdouts_6[38]));
    v.set_bit(39, vl::rd(testdouts_6[39]));
    v.set_bit(40, vl::rd(testdouts_6[40]));
    v.set_bit(41, vl::rd(testdouts_6[41]));
    v.set_bit(42, vl::rd(testdouts_6[42]));
    v.set_bit(43, vl::rd(testdouts_6[43]));
    v.set_bit(44, vl::rd(testdouts_6[44]));
    v.set_bit(45, vl::rd(testdouts_6[45]));
    v.set_bit(46, vl::rd(testdouts_6[46]));
    v.set_bit(47, vl::rd(testdouts_6[47]));
    v.set_bit(48, vl::rd(testdouts_6[48]));
    v.set_bit(49, vl::rd(testdouts_6[49]));
    v.set_bit(50, vl::rd(testdouts_6[50]));
    v.set_bit(51, vl::rd(testdouts_6[51]));
    v.set_bit(52, vl::rd(testdouts_6[52]));
    v.set_bit(53, vl::rd(testdouts_6[53]));
    v.set_bit(54, vl::rd(testdouts_6[54]));
    v.set_bit(55, vl::rd(testdouts_6[55]));
    v.set_bit(56, vl::rd(testdouts_6[56]));
    v.set_bit(57, vl::rd(testdouts_6[57]));
    v.set_bit(58, vl::rd(testdouts_6[58]));
    v.set_bit(59, vl::rd(testdouts_6[59]));
    v.set_bit(60, vl::rd(testdouts_6[60]));
    v.set_bit(61, vl::rd(testdouts_6[61]));
    v.set_bit(62, vl::rd(testdouts_6[62]));
    v.set_bit(63, vl::rd(testdouts_6[63]));
    v.set_bit(64, vl::rd(testdouts_6[64]));
    v.set_bit(65, vl::rd(testdouts_6[65]));
    v.set_bit(66, vl::rd(testdouts_6[66]));
    v.set_bit(67, vl::rd(testdouts_6[67]));
    v.set_bit(68, vl::rd(testdouts_6[68]));
    v.set_bit(69, vl::rd(testdouts_6[69]));
    v.set_bit(70, vl::rd(testdouts_6[70]));
    v.set_bit(71, vl::rd(testdouts_6[71]));
    v.set_bit(72, vl::rd(testdouts_6[72]));
    v.set_bit(73, vl::rd(testdouts_6[73]));
    v.set_bit(74, vl::rd(testdouts_6[74]));
    v.set_bit(75, vl::rd(testdouts_6[75]));
    v.set_bit(76, vl::rd(testdouts_6[76]));
    v.set_bit(77, vl::rd(testdouts_6[77]));
    v.set_bit(78, vl::rd(testdouts_6[78]));
    v.set_bit(79, vl::rd(testdouts_6[79]));
    v.set_bit(80, vl::rd(testdouts_6[80]));
    v.set_bit(81, vl::rd(testdouts_6[81]));
    v.set_bit(82, vl::rd(testdouts_6[82]));
    v.set_bit(83, vl::rd(testdouts_6[83]));
    v.set_bit(84, vl::rd(testdouts_6[84]));
    v.set_bit(85, vl::rd(testdouts_6[85]));
    v.set_bit(86, vl::rd(testdouts_6[86]));
    v.set_bit(87, vl::rd(testdouts_6[87]));
    v.set_bit(88, vl::rd(testdouts_6[88]));
    v.set_bit(89, vl::rd(testdouts_6[89]));
    v.set_bit(90, vl::rd(testdouts_6[90]));
    v.set_bit(91, vl::rd(testdouts_6[91]));
    v.set_bit(92, vl::rd(testdouts_6[92]));
    v.set_bit(93, vl::rd(testdouts_6[93]));
    v.set_bit(94, vl::rd(testdouts_6[94]));
    v.set_bit(95, vl::rd(testdouts_6[95]));
    v.set_bit(96, vl::rd(testdouts_6[96]));
    v.set_bit(97, vl::rd(testdouts_6[97]));
    v.set_bit(98, vl::rd(testdouts_6[98]));
    v.set_bit(99, vl::rd(testdouts_6[99]));
    v.set_bit(100, vl::rd(testdouts_6[100]));
    v.set_bit(101, vl::rd(testdouts_6[101]));
    v.set_bit(102, vl::rd(testdouts_6[102]));
    v.set_bit(103, vl::rd(testdouts_6[103]));
    v.set_bit(104, vl::rd(testdouts_6[104]));
    v.set_bit(105, vl::rd(testdouts_6[105]));
    v.set_bit(106, vl::rd(testdouts_6[106]));
    v.set_bit(107, vl::rd(testdouts_6[107]));
    v.set_bit(108, vl::rd(testdouts_6[108]));
    v.set_bit(109, vl::rd(testdouts_6[109]));
    v.set_bit(110, vl::rd(testdouts_6[110]));
    v.set_bit(111, vl::rd(testdouts_6[111]));
    v.set_bit(112, vl::rd(testdouts_6[112]));
    v.set_bit(113, vl::rd(testdouts_6[113]));
    v.set_bit(114, vl::rd(testdouts_6[114]));
    v.set_bit(115, vl::rd(testdouts_6[115]));
    v.set_bit(116, vl::rd(testdouts_6[116]));
    v.set_bit(117, vl::rd(testdouts_6[117]));
    v.set_bit(118, vl::rd(testdouts_6[118]));
    v.set_bit(119, vl::rd(testdouts_6[119]));
    v.set_bit(120, vl::rd(testdouts_6[120]));
    v.set_bit(121, vl::rd(testdouts_6[121]));
    v.set_bit(122, vl::rd(testdouts_6[122]));
    v.set_bit(123, vl::rd(testdouts_6[123]));
    v.set_bit(124, vl::rd(testdouts_6[124]));
    v.set_bit(125, vl::rd(testdouts_6[125]));
    v.set_bit(126, vl::rd(testdouts_6[126]));
    v.set_bit(127, vl::rd(testdouts_6[127]));
    v.set_bit(128, vl::rd(testdouts_6[128]));
    v.set_bit(129, vl::rd(testdouts_6[129]));
    v.set_bit(130, vl::rd(testdouts_6[130]));
    v.set_bit(131, vl::rd(testdouts_6[131]));
    v.set_bit(132, vl::rd(testdouts_6[132]));
    v.set_bit(133, vl::rd(testdouts_6[133]));
    v.set_bit(134, vl::rd(testdouts_6[134]));
    v.set_bit(135, vl::rd(testdouts_6[135]));
    v.set_bit(136, vl::rd(testdouts_6[136]));
    v.set_bit(137, vl::rd(testdouts_6[137]));
    v.set_bit(138, vl::rd(testdouts_6[138]));
    v.set_bit(139, vl::rd(testdouts_6[139]));
    v.set_bit(140, vl::rd(testdouts_6[140]));
    v.set_bit(141, vl::rd(testdouts_6[141]));
    v.set_bit(142, vl::rd(testdouts_6[142]));
    v.set_bit(143, vl::rd(testdouts_6[143]));
    v.set_bit(144, vl::rd(testdouts_6[144]));
    v.set_bit(145, vl::rd(testdouts_6[145]));
    v.set_bit(146, vl::rd(testdouts_6[146]));
    v.set_bit(147, vl::rd(testdouts_6[147]));
    v.set_bit(148, vl::rd(testdouts_6[148]));
    v.set_bit(149, vl::rd(testdouts_6[149]));
    v.set_bit(150, vl::rd(testdouts_6[150]));
    v.set_bit(151, vl::rd(testdouts_6[151]));
    v.set_bit(152, vl::rd(testdouts_6[152]));
    v.set_bit(153, vl::rd(testdouts_6[153]));
    v.set_bit(154, vl::rd(testdouts_6[154]));
    v.set_bit(155, vl::rd(testdouts_6[155]));
    v.set_bit(156, vl::rd(testdouts_6[156]));
    v.set_bit(157, vl::rd(testdouts_6[157]));
    v.set_bit(158, vl::rd(testdouts_6[158]));
    v.set_bit(159, vl::rd(testdouts_6[159]));
    v.set_bit(160, vl::rd(testdouts_6[160]));
    v.set_bit(161, vl::rd(testdouts_6[161]));
    v.set_bit(162, vl::rd(testdouts_6[162]));
    v.set_bit(163, vl::rd(testdouts_6[163]));
    v.set_bit(164, vl::rd(testdouts_6[164]));
    v.set_bit(165, vl::rd(testdouts_6[165]));
    v.set_bit(166, vl::rd(testdouts_6[166]));
    v.set_bit(167, vl::rd(testdouts_6[167]));
    v.set_bit(168, vl::rd(testdouts_6[168]));
    v.set_bit(169, vl::rd(testdouts_6[169]));
    v.set_bit(170, vl::rd(testdouts_6[170]));
    v.set_bit(171, vl::rd(testdouts_6[171]));
    v.set_bit(172, vl::rd(testdouts_6[172]));
    v.set_bit(173, vl::rd(testdouts_6[173]));
    v.set_bit(174, vl::rd(testdouts_6[174]));
    v.set_bit(175, vl::rd(testdouts_6[175]));
    v.set_bit(176, vl::rd(testdouts_6[176]));
    v.set_bit(177, vl::rd(testdouts_6[177]));
    v.set_bit(178, vl::rd(testdouts_6[178]));
    v.set_bit(179, vl::rd(testdouts_6[179]));
    v.set_bit(180, vl::rd(testdouts_6[180]));
    v.set_bit(181, vl::rd(testdouts_6[181]));
    v.set_bit(182, vl::rd(testdouts_6[182]));
    v.set_bit(183, vl::rd(testdouts_6[183]));
    v.set_bit(184, vl::rd(testdouts_6[184]));
    v.set_bit(185, vl::rd(testdouts_6[185]));
    v.set_bit(186, vl::rd(testdouts_6[186]));
    v.set_bit(187, vl::rd(testdouts_6[187]));
    v.set_bit(188, vl::rd(testdouts_6[188]));
    v.set_bit(189, vl::rd(testdouts_6[189]));
    v.set_bit(190, vl::rd(testdouts_6[190]));
    v.set_bit(191, vl::rd(testdouts_6[191]));
    v.set_bit(192, vl::rd(testdouts_6[192]));
    v.set_bit(193, vl::rd(testdouts_6[193]));
    v.set_bit(194, vl::rd(testdouts_6[194]));
    v.set_bit(195, vl::rd(testdouts_6[195]));
    v.set_bit(196, vl::rd(testdouts_6[196]));
    v.set_bit(197, vl::rd(testdouts_6[197]));
    v.set_bit(198, vl::rd(testdouts_6[198]));
    v.set_bit(199, vl::rd(testdouts_6[199]));
    v.set_bit(200, vl::rd(testdouts_6[200]));
    v.set_bit(201, vl::rd(testdouts_6[201]));
    v.set_bit(202, vl::rd(testdouts_6[202]));
    v.set_bit(203, vl::rd(testdouts_6[203]));
    v.set_bit(204, vl::rd(testdouts_6[204]));
    v.set_bit(205, vl::rd(testdouts_6[205]));
    v.set_bit(206, vl::rd(testdouts_6[206]));
    v.set_bit(207, vl::rd(testdouts_6[207]));
    v.set_bit(208, vl::rd(testdouts_6[208]));
    v.set_bit(209, vl::rd(testdouts_6[209]));
    v.set_bit(210, vl::rd(testdouts_6[210]));
    v.set_bit(211, vl::rd(testdouts_6[211]));
    v.set_bit(212, vl::rd(testdouts_6[212]));
    v.set_bit(213, vl::rd(testdouts_6[213]));
    v.set_bit(214, vl::rd(testdouts_6[214]));
    v.set_bit(215, vl::rd(testdouts_6[215]));
    v.set_bit(216, vl::rd(testdouts_6[216]));
    v.set_bit(217, vl::rd(testdouts_6[217]));
    v.set_bit(218, vl::rd(testdouts_6[218]));
    v.set_bit(219, vl::rd(testdouts_6[219]));
    v.set_bit(220, vl::rd(testdouts_6[220]));
    v.set_bit(221, vl::rd(testdouts_6[221]));
    v.set_bit(222, vl::rd(testdouts_6[222]));
    v.set_bit(223, vl::rd(testdouts_6[223]));
    v.set_bit(224, vl::rd(testdouts_6[224]));
    v.set_bit(225, vl::rd(testdouts_6[225]));
    v.set_bit(226, vl::rd(testdouts_6[226]));
    v.set_bit(227, vl::rd(testdouts_6[227]));
    v.set_bit(228, vl::rd(testdouts_6[228]));
    v.set_bit(229, vl::rd(testdouts_6[229]));
    v.set_bit(230, vl::rd(testdouts_6[230]));
    v.set_bit(231, vl::rd(testdouts_6[231]));
    v.set_bit(232, vl::rd(testdouts_6[232]));
    v.set_bit(233, vl::rd(testdouts_6[233]));
    v.set_bit(234, vl::rd(testdouts_6[234]));
    v.set_bit(235, vl::rd(testdouts_6[235]));
    v.set_bit(236, vl::rd(testdouts_6[236]));
    v.set_bit(237, vl::rd(testdouts_6[237]));
    v.set_bit(238, vl::rd(testdouts_6[238]));
    v.set_bit(239, vl::rd(testdouts_6[239]));
    v.set_bit(240, vl::rd(testdouts_6[240]));
    v.set_bit(241, vl::rd(testdouts_6[241]));
    v.set_bit(242, vl::rd(testdouts_6[242]));
    v.set_bit(243, vl::rd(testdouts_6[243]));
    v.set_bit(244, vl::rd(testdouts_6[244]));
    v.set_bit(245, vl::rd(testdouts_6[245]));
    v.set_bit(246, vl::rd(testdouts_6[246]));
    v.set_bit(247, vl::rd(testdouts_6[247]));
    v.set_bit(248, vl::rd(testdouts_6[248]));
    v.set_bit(249, vl::rd(testdouts_6[249]));
    v.set_bit(250, vl::rd(testdouts_6[250]));
    v.set_bit(251, vl::rd(testdouts_6[251]));
    v.set_bit(252, vl::rd(testdouts_6[252]));
    v.set_bit(253, vl::rd(testdouts_6[253]));
    v.set_bit(254, vl::rd(testdouts_6[254]));
    v.set_bit(255, vl::rd(testdouts_6[255]));
    v.set_bit(256, vl::rd(testdouts_6[256]));
    v.set_bit(257, vl::rd(testdouts_6[257]));
    v.set_bit(258, vl::rd(testdouts_6[258]));
    v.set_bit(259, vl::rd(testdouts_6[259]));
    v.set_bit(260, vl::rd(testdouts_6[260]));
    v.set_bit(261, vl::rd(testdouts_6[261]));
    v.set_bit(262, vl::rd(testdouts_6[262]));
    v.set_bit(263, vl::rd(testdouts_6[263]));
    v.set_bit(264, vl::rd(testdouts_6[264]));
    v.set_bit(265, vl::rd(testdouts_6[265]));
    v.set_bit(266, vl::rd(testdouts_6[266]));
    v.set_bit(267, vl::rd(testdouts_6[267]));
    v.set_bit(268, vl::rd(testdouts_6[268]));
    v.set_bit(269, vl::rd(testdouts_6[269]));
    v.set_bit(270, vl::rd(testdouts_6[270]));
    v.set_bit(271, vl::rd(testdouts_6[271]));
    v.set_bit(272, vl::rd(testdouts_6[272]));
    v.set_bit(273, vl::rd(testdouts_6[273]));
    v.set_bit(274, vl::rd(testdouts_6[274]));
    v.set_bit(275, vl::rd(testdouts_6[275]));
    v.set_bit(276, vl::rd(testdouts_6[276]));
    v.set_bit(277, vl::rd(testdouts_6[277]));
    v.set_bit(278, vl::rd(testdouts_6[278]));
    v.set_bit(279, vl::rd(testdouts_6[279]));
    v.set_bit(280, vl::rd(testdouts_6[280]));
    v.set_bit(281, vl::rd(testdouts_6[281]));
    v.set_bit(282, vl::rd(testdouts_6[282]));
    v.set_bit(283, vl::rd(testdouts_6[283]));
    v.set_bit(284, vl::rd(testdouts_6[284]));
    v.set_bit(285, vl::rd(testdouts_6[285]));
    v.set_bit(286, vl::rd(testdouts_6[286]));
    v.set_bit(287, vl::rd(testdouts_6[287]));
    v.set_bit(288, vl::rd(testdouts_6[288]));
    v.set_bit(289, vl::rd(testdouts_6[289]));
    v.set_bit(290, vl::rd(testdouts_6[290]));
    v.set_bit(291, vl::rd(testdouts_6[291]));
    v.set_bit(292, vl::rd(testdouts_6[292]));
    v.set_bit(293, vl::rd(testdouts_6[293]));
    v.set_bit(294, vl::rd(testdouts_6[294]));
    v.set_bit(295, vl::rd(testdouts_6[295]));
    v.set_bit(296, vl::rd(testdouts_6[296]));
    v.set_bit(297, vl::rd(testdouts_6[297]));
    v.set_bit(298, vl::rd(testdouts_6[298]));
    v.set_bit(299, vl::rd(testdouts_6[299]));
    v.set_bit(300, vl::rd(testdouts_6[300]));
    v.set_bit(301, vl::rd(testdouts_6[301]));
    v.set_bit(302, vl::rd(testdouts_6[302]));
    v.set_bit(303, vl::rd(testdouts_6[303]));
    v.set_bit(304, vl::rd(testdouts_6[304]));
    v.set_bit(305, vl::rd(testdouts_6[305]));
    v.set_bit(306, vl::rd(testdouts_6[306]));
    v.set_bit(307, vl::rd(testdouts_6[307]));
    v.set_bit(308, vl::rd(testdouts_6[308]));
    v.set_bit(309, vl::rd(testdouts_6[309]));
    v.set_bit(310, vl::rd(testdouts_6[310]));
    v.set_bit(311, vl::rd(testdouts_6[311]));
    v.set_bit(312, vl::rd(testdouts_6[312]));
    v.set_bit(313, vl::rd(testdouts_6[313]));
    v.set_bit(314, vl::rd(testdouts_6[314]));
    return v;
}

vl::logic<315> mipi_4_rx_dphy_ns_dphy4rxtester::rd_testdouts_7() const {
    vl::logic<315> v(0);
    v.set_bit(0, vl::rd(testdouts_7[0]));
    v.set_bit(1, vl::rd(testdouts_7[1]));
    v.set_bit(2, vl::rd(testdouts_7[2]));
    v.set_bit(3, vl::rd(testdouts_7[3]));
    v.set_bit(4, vl::rd(testdouts_7[4]));
    v.set_bit(5, vl::rd(testdouts_7[5]));
    v.set_bit(6, vl::rd(testdouts_7[6]));
    v.set_bit(7, vl::rd(testdouts_7[7]));
    v.set_bit(8, vl::rd(testdouts_7[8]));
    v.set_bit(9, vl::rd(testdouts_7[9]));
    v.set_bit(10, vl::rd(testdouts_7[10]));
    v.set_bit(11, vl::rd(testdouts_7[11]));
    v.set_bit(12, vl::rd(testdouts_7[12]));
    v.set_bit(13, vl::rd(testdouts_7[13]));
    v.set_bit(14, vl::rd(testdouts_7[14]));
    v.set_bit(15, vl::rd(testdouts_7[15]));
    v.set_bit(16, vl::rd(testdouts_7[16]));
    v.set_bit(17, vl::rd(testdouts_7[17]));
    v.set_bit(18, vl::rd(testdouts_7[18]));
    v.set_bit(19, vl::rd(testdouts_7[19]));
    v.set_bit(20, vl::rd(testdouts_7[20]));
    v.set_bit(21, vl::rd(testdouts_7[21]));
    v.set_bit(22, vl::rd(testdouts_7[22]));
    v.set_bit(23, vl::rd(testdouts_7[23]));
    v.set_bit(24, vl::rd(testdouts_7[24]));
    v.set_bit(25, vl::rd(testdouts_7[25]));
    v.set_bit(26, vl::rd(testdouts_7[26]));
    v.set_bit(27, vl::rd(testdouts_7[27]));
    v.set_bit(28, vl::rd(testdouts_7[28]));
    v.set_bit(29, vl::rd(testdouts_7[29]));
    v.set_bit(30, vl::rd(testdouts_7[30]));
    v.set_bit(31, vl::rd(testdouts_7[31]));
    v.set_bit(32, vl::rd(testdouts_7[32]));
    v.set_bit(33, vl::rd(testdouts_7[33]));
    v.set_bit(34, vl::rd(testdouts_7[34]));
    v.set_bit(35, vl::rd(testdouts_7[35]));
    v.set_bit(36, vl::rd(testdouts_7[36]));
    v.set_bit(37, vl::rd(testdouts_7[37]));
    v.set_bit(38, vl::rd(testdouts_7[38]));
    v.set_bit(39, vl::rd(testdouts_7[39]));
    v.set_bit(40, vl::rd(testdouts_7[40]));
    v.set_bit(41, vl::rd(testdouts_7[41]));
    v.set_bit(42, vl::rd(testdouts_7[42]));
    v.set_bit(43, vl::rd(testdouts_7[43]));
    v.set_bit(44, vl::rd(testdouts_7[44]));
    v.set_bit(45, vl::rd(testdouts_7[45]));
    v.set_bit(46, vl::rd(testdouts_7[46]));
    v.set_bit(47, vl::rd(testdouts_7[47]));
    v.set_bit(48, vl::rd(testdouts_7[48]));
    v.set_bit(49, vl::rd(testdouts_7[49]));
    v.set_bit(50, vl::rd(testdouts_7[50]));
    v.set_bit(51, vl::rd(testdouts_7[51]));
    v.set_bit(52, vl::rd(testdouts_7[52]));
    v.set_bit(53, vl::rd(testdouts_7[53]));
    v.set_bit(54, vl::rd(testdouts_7[54]));
    v.set_bit(55, vl::rd(testdouts_7[55]));
    v.set_bit(56, vl::rd(testdouts_7[56]));
    v.set_bit(57, vl::rd(testdouts_7[57]));
    v.set_bit(58, vl::rd(testdouts_7[58]));
    v.set_bit(59, vl::rd(testdouts_7[59]));
    v.set_bit(60, vl::rd(testdouts_7[60]));
    v.set_bit(61, vl::rd(testdouts_7[61]));
    v.set_bit(62, vl::rd(testdouts_7[62]));
    v.set_bit(63, vl::rd(testdouts_7[63]));
    v.set_bit(64, vl::rd(testdouts_7[64]));
    v.set_bit(65, vl::rd(testdouts_7[65]));
    v.set_bit(66, vl::rd(testdouts_7[66]));
    v.set_bit(67, vl::rd(testdouts_7[67]));
    v.set_bit(68, vl::rd(testdouts_7[68]));
    v.set_bit(69, vl::rd(testdouts_7[69]));
    v.set_bit(70, vl::rd(testdouts_7[70]));
    v.set_bit(71, vl::rd(testdouts_7[71]));
    v.set_bit(72, vl::rd(testdouts_7[72]));
    v.set_bit(73, vl::rd(testdouts_7[73]));
    v.set_bit(74, vl::rd(testdouts_7[74]));
    v.set_bit(75, vl::rd(testdouts_7[75]));
    v.set_bit(76, vl::rd(testdouts_7[76]));
    v.set_bit(77, vl::rd(testdouts_7[77]));
    v.set_bit(78, vl::rd(testdouts_7[78]));
    v.set_bit(79, vl::rd(testdouts_7[79]));
    v.set_bit(80, vl::rd(testdouts_7[80]));
    v.set_bit(81, vl::rd(testdouts_7[81]));
    v.set_bit(82, vl::rd(testdouts_7[82]));
    v.set_bit(83, vl::rd(testdouts_7[83]));
    v.set_bit(84, vl::rd(testdouts_7[84]));
    v.set_bit(85, vl::rd(testdouts_7[85]));
    v.set_bit(86, vl::rd(testdouts_7[86]));
    v.set_bit(87, vl::rd(testdouts_7[87]));
    v.set_bit(88, vl::rd(testdouts_7[88]));
    v.set_bit(89, vl::rd(testdouts_7[89]));
    v.set_bit(90, vl::rd(testdouts_7[90]));
    v.set_bit(91, vl::rd(testdouts_7[91]));
    v.set_bit(92, vl::rd(testdouts_7[92]));
    v.set_bit(93, vl::rd(testdouts_7[93]));
    v.set_bit(94, vl::rd(testdouts_7[94]));
    v.set_bit(95, vl::rd(testdouts_7[95]));
    v.set_bit(96, vl::rd(testdouts_7[96]));
    v.set_bit(97, vl::rd(testdouts_7[97]));
    v.set_bit(98, vl::rd(testdouts_7[98]));
    v.set_bit(99, vl::rd(testdouts_7[99]));
    v.set_bit(100, vl::rd(testdouts_7[100]));
    v.set_bit(101, vl::rd(testdouts_7[101]));
    v.set_bit(102, vl::rd(testdouts_7[102]));
    v.set_bit(103, vl::rd(testdouts_7[103]));
    v.set_bit(104, vl::rd(testdouts_7[104]));
    v.set_bit(105, vl::rd(testdouts_7[105]));
    v.set_bit(106, vl::rd(testdouts_7[106]));
    v.set_bit(107, vl::rd(testdouts_7[107]));
    v.set_bit(108, vl::rd(testdouts_7[108]));
    v.set_bit(109, vl::rd(testdouts_7[109]));
    v.set_bit(110, vl::rd(testdouts_7[110]));
    v.set_bit(111, vl::rd(testdouts_7[111]));
    v.set_bit(112, vl::rd(testdouts_7[112]));
    v.set_bit(113, vl::rd(testdouts_7[113]));
    v.set_bit(114, vl::rd(testdouts_7[114]));
    v.set_bit(115, vl::rd(testdouts_7[115]));
    v.set_bit(116, vl::rd(testdouts_7[116]));
    v.set_bit(117, vl::rd(testdouts_7[117]));
    v.set_bit(118, vl::rd(testdouts_7[118]));
    v.set_bit(119, vl::rd(testdouts_7[119]));
    v.set_bit(120, vl::rd(testdouts_7[120]));
    v.set_bit(121, vl::rd(testdouts_7[121]));
    v.set_bit(122, vl::rd(testdouts_7[122]));
    v.set_bit(123, vl::rd(testdouts_7[123]));
    v.set_bit(124, vl::rd(testdouts_7[124]));
    v.set_bit(125, vl::rd(testdouts_7[125]));
    v.set_bit(126, vl::rd(testdouts_7[126]));
    v.set_bit(127, vl::rd(testdouts_7[127]));
    v.set_bit(128, vl::rd(testdouts_7[128]));
    v.set_bit(129, vl::rd(testdouts_7[129]));
    v.set_bit(130, vl::rd(testdouts_7[130]));
    v.set_bit(131, vl::rd(testdouts_7[131]));
    v.set_bit(132, vl::rd(testdouts_7[132]));
    v.set_bit(133, vl::rd(testdouts_7[133]));
    v.set_bit(134, vl::rd(testdouts_7[134]));
    v.set_bit(135, vl::rd(testdouts_7[135]));
    v.set_bit(136, vl::rd(testdouts_7[136]));
    v.set_bit(137, vl::rd(testdouts_7[137]));
    v.set_bit(138, vl::rd(testdouts_7[138]));
    v.set_bit(139, vl::rd(testdouts_7[139]));
    v.set_bit(140, vl::rd(testdouts_7[140]));
    v.set_bit(141, vl::rd(testdouts_7[141]));
    v.set_bit(142, vl::rd(testdouts_7[142]));
    v.set_bit(143, vl::rd(testdouts_7[143]));
    v.set_bit(144, vl::rd(testdouts_7[144]));
    v.set_bit(145, vl::rd(testdouts_7[145]));
    v.set_bit(146, vl::rd(testdouts_7[146]));
    v.set_bit(147, vl::rd(testdouts_7[147]));
    v.set_bit(148, vl::rd(testdouts_7[148]));
    v.set_bit(149, vl::rd(testdouts_7[149]));
    v.set_bit(150, vl::rd(testdouts_7[150]));
    v.set_bit(151, vl::rd(testdouts_7[151]));
    v.set_bit(152, vl::rd(testdouts_7[152]));
    v.set_bit(153, vl::rd(testdouts_7[153]));
    v.set_bit(154, vl::rd(testdouts_7[154]));
    v.set_bit(155, vl::rd(testdouts_7[155]));
    v.set_bit(156, vl::rd(testdouts_7[156]));
    v.set_bit(157, vl::rd(testdouts_7[157]));
    v.set_bit(158, vl::rd(testdouts_7[158]));
    v.set_bit(159, vl::rd(testdouts_7[159]));
    v.set_bit(160, vl::rd(testdouts_7[160]));
    v.set_bit(161, vl::rd(testdouts_7[161]));
    v.set_bit(162, vl::rd(testdouts_7[162]));
    v.set_bit(163, vl::rd(testdouts_7[163]));
    v.set_bit(164, vl::rd(testdouts_7[164]));
    v.set_bit(165, vl::rd(testdouts_7[165]));
    v.set_bit(166, vl::rd(testdouts_7[166]));
    v.set_bit(167, vl::rd(testdouts_7[167]));
    v.set_bit(168, vl::rd(testdouts_7[168]));
    v.set_bit(169, vl::rd(testdouts_7[169]));
    v.set_bit(170, vl::rd(testdouts_7[170]));
    v.set_bit(171, vl::rd(testdouts_7[171]));
    v.set_bit(172, vl::rd(testdouts_7[172]));
    v.set_bit(173, vl::rd(testdouts_7[173]));
    v.set_bit(174, vl::rd(testdouts_7[174]));
    v.set_bit(175, vl::rd(testdouts_7[175]));
    v.set_bit(176, vl::rd(testdouts_7[176]));
    v.set_bit(177, vl::rd(testdouts_7[177]));
    v.set_bit(178, vl::rd(testdouts_7[178]));
    v.set_bit(179, vl::rd(testdouts_7[179]));
    v.set_bit(180, vl::rd(testdouts_7[180]));
    v.set_bit(181, vl::rd(testdouts_7[181]));
    v.set_bit(182, vl::rd(testdouts_7[182]));
    v.set_bit(183, vl::rd(testdouts_7[183]));
    v.set_bit(184, vl::rd(testdouts_7[184]));
    v.set_bit(185, vl::rd(testdouts_7[185]));
    v.set_bit(186, vl::rd(testdouts_7[186]));
    v.set_bit(187, vl::rd(testdouts_7[187]));
    v.set_bit(188, vl::rd(testdouts_7[188]));
    v.set_bit(189, vl::rd(testdouts_7[189]));
    v.set_bit(190, vl::rd(testdouts_7[190]));
    v.set_bit(191, vl::rd(testdouts_7[191]));
    v.set_bit(192, vl::rd(testdouts_7[192]));
    v.set_bit(193, vl::rd(testdouts_7[193]));
    v.set_bit(194, vl::rd(testdouts_7[194]));
    v.set_bit(195, vl::rd(testdouts_7[195]));
    v.set_bit(196, vl::rd(testdouts_7[196]));
    v.set_bit(197, vl::rd(testdouts_7[197]));
    v.set_bit(198, vl::rd(testdouts_7[198]));
    v.set_bit(199, vl::rd(testdouts_7[199]));
    v.set_bit(200, vl::rd(testdouts_7[200]));
    v.set_bit(201, vl::rd(testdouts_7[201]));
    v.set_bit(202, vl::rd(testdouts_7[202]));
    v.set_bit(203, vl::rd(testdouts_7[203]));
    v.set_bit(204, vl::rd(testdouts_7[204]));
    v.set_bit(205, vl::rd(testdouts_7[205]));
    v.set_bit(206, vl::rd(testdouts_7[206]));
    v.set_bit(207, vl::rd(testdouts_7[207]));
    v.set_bit(208, vl::rd(testdouts_7[208]));
    v.set_bit(209, vl::rd(testdouts_7[209]));
    v.set_bit(210, vl::rd(testdouts_7[210]));
    v.set_bit(211, vl::rd(testdouts_7[211]));
    v.set_bit(212, vl::rd(testdouts_7[212]));
    v.set_bit(213, vl::rd(testdouts_7[213]));
    v.set_bit(214, vl::rd(testdouts_7[214]));
    v.set_bit(215, vl::rd(testdouts_7[215]));
    v.set_bit(216, vl::rd(testdouts_7[216]));
    v.set_bit(217, vl::rd(testdouts_7[217]));
    v.set_bit(218, vl::rd(testdouts_7[218]));
    v.set_bit(219, vl::rd(testdouts_7[219]));
    v.set_bit(220, vl::rd(testdouts_7[220]));
    v.set_bit(221, vl::rd(testdouts_7[221]));
    v.set_bit(222, vl::rd(testdouts_7[222]));
    v.set_bit(223, vl::rd(testdouts_7[223]));
    v.set_bit(224, vl::rd(testdouts_7[224]));
    v.set_bit(225, vl::rd(testdouts_7[225]));
    v.set_bit(226, vl::rd(testdouts_7[226]));
    v.set_bit(227, vl::rd(testdouts_7[227]));
    v.set_bit(228, vl::rd(testdouts_7[228]));
    v.set_bit(229, vl::rd(testdouts_7[229]));
    v.set_bit(230, vl::rd(testdouts_7[230]));
    v.set_bit(231, vl::rd(testdouts_7[231]));
    v.set_bit(232, vl::rd(testdouts_7[232]));
    v.set_bit(233, vl::rd(testdouts_7[233]));
    v.set_bit(234, vl::rd(testdouts_7[234]));
    v.set_bit(235, vl::rd(testdouts_7[235]));
    v.set_bit(236, vl::rd(testdouts_7[236]));
    v.set_bit(237, vl::rd(testdouts_7[237]));
    v.set_bit(238, vl::rd(testdouts_7[238]));
    v.set_bit(239, vl::rd(testdouts_7[239]));
    v.set_bit(240, vl::rd(testdouts_7[240]));
    v.set_bit(241, vl::rd(testdouts_7[241]));
    v.set_bit(242, vl::rd(testdouts_7[242]));
    v.set_bit(243, vl::rd(testdouts_7[243]));
    v.set_bit(244, vl::rd(testdouts_7[244]));
    v.set_bit(245, vl::rd(testdouts_7[245]));
    v.set_bit(246, vl::rd(testdouts_7[246]));
    v.set_bit(247, vl::rd(testdouts_7[247]));
    v.set_bit(248, vl::rd(testdouts_7[248]));
    v.set_bit(249, vl::rd(testdouts_7[249]));
    v.set_bit(250, vl::rd(testdouts_7[250]));
    v.set_bit(251, vl::rd(testdouts_7[251]));
    v.set_bit(252, vl::rd(testdouts_7[252]));
    v.set_bit(253, vl::rd(testdouts_7[253]));
    v.set_bit(254, vl::rd(testdouts_7[254]));
    v.set_bit(255, vl::rd(testdouts_7[255]));
    v.set_bit(256, vl::rd(testdouts_7[256]));
    v.set_bit(257, vl::rd(testdouts_7[257]));
    v.set_bit(258, vl::rd(testdouts_7[258]));
    v.set_bit(259, vl::rd(testdouts_7[259]));
    v.set_bit(260, vl::rd(testdouts_7[260]));
    v.set_bit(261, vl::rd(testdouts_7[261]));
    v.set_bit(262, vl::rd(testdouts_7[262]));
    v.set_bit(263, vl::rd(testdouts_7[263]));
    v.set_bit(264, vl::rd(testdouts_7[264]));
    v.set_bit(265, vl::rd(testdouts_7[265]));
    v.set_bit(266, vl::rd(testdouts_7[266]));
    v.set_bit(267, vl::rd(testdouts_7[267]));
    v.set_bit(268, vl::rd(testdouts_7[268]));
    v.set_bit(269, vl::rd(testdouts_7[269]));
    v.set_bit(270, vl::rd(testdouts_7[270]));
    v.set_bit(271, vl::rd(testdouts_7[271]));
    v.set_bit(272, vl::rd(testdouts_7[272]));
    v.set_bit(273, vl::rd(testdouts_7[273]));
    v.set_bit(274, vl::rd(testdouts_7[274]));
    v.set_bit(275, vl::rd(testdouts_7[275]));
    v.set_bit(276, vl::rd(testdouts_7[276]));
    v.set_bit(277, vl::rd(testdouts_7[277]));
    v.set_bit(278, vl::rd(testdouts_7[278]));
    v.set_bit(279, vl::rd(testdouts_7[279]));
    v.set_bit(280, vl::rd(testdouts_7[280]));
    v.set_bit(281, vl::rd(testdouts_7[281]));
    v.set_bit(282, vl::rd(testdouts_7[282]));
    v.set_bit(283, vl::rd(testdouts_7[283]));
    v.set_bit(284, vl::rd(testdouts_7[284]));
    v.set_bit(285, vl::rd(testdouts_7[285]));
    v.set_bit(286, vl::rd(testdouts_7[286]));
    v.set_bit(287, vl::rd(testdouts_7[287]));
    v.set_bit(288, vl::rd(testdouts_7[288]));
    v.set_bit(289, vl::rd(testdouts_7[289]));
    v.set_bit(290, vl::rd(testdouts_7[290]));
    v.set_bit(291, vl::rd(testdouts_7[291]));
    v.set_bit(292, vl::rd(testdouts_7[292]));
    v.set_bit(293, vl::rd(testdouts_7[293]));
    v.set_bit(294, vl::rd(testdouts_7[294]));
    v.set_bit(295, vl::rd(testdouts_7[295]));
    v.set_bit(296, vl::rd(testdouts_7[296]));
    v.set_bit(297, vl::rd(testdouts_7[297]));
    v.set_bit(298, vl::rd(testdouts_7[298]));
    v.set_bit(299, vl::rd(testdouts_7[299]));
    v.set_bit(300, vl::rd(testdouts_7[300]));
    v.set_bit(301, vl::rd(testdouts_7[301]));
    v.set_bit(302, vl::rd(testdouts_7[302]));
    v.set_bit(303, vl::rd(testdouts_7[303]));
    v.set_bit(304, vl::rd(testdouts_7[304]));
    v.set_bit(305, vl::rd(testdouts_7[305]));
    v.set_bit(306, vl::rd(testdouts_7[306]));
    v.set_bit(307, vl::rd(testdouts_7[307]));
    v.set_bit(308, vl::rd(testdouts_7[308]));
    v.set_bit(309, vl::rd(testdouts_7[309]));
    v.set_bit(310, vl::rd(testdouts_7[310]));
    v.set_bit(311, vl::rd(testdouts_7[311]));
    v.set_bit(312, vl::rd(testdouts_7[312]));
    v.set_bit(313, vl::rd(testdouts_7[313]));
    v.set_bit(314, vl::rd(testdouts_7[314]));
    return v;
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_xaddr() {
    vl::wr(xaddr, vl::cat(vl::rd(testcode_x), vl::rd(testcode)));
}

// OR all testdouts
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdout() {
    const vl::logic<1> v_ = vl::ror_(rd_testdouts_0()) | (vl::rd(testcode_x).bit(0) & vl::eq(vl::rd(testcode), vl::logic<8>(0x0ULL)));
    testdout_b[0].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdout_2() {
    const vl::logic<1> v_ = vl::ror_(rd_testdouts_1()) | (vl::rd(testcode_x).bit(1) & vl::eq(vl::rd(testcode), vl::logic<8>(0x0ULL)));
    testdout_b[1].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdout_3() {
    const vl::logic<1> v_ = vl::ror_(rd_testdouts_2()) | (vl::rd(testcode_x).bit(2) & vl::eq(vl::rd(testcode), vl::logic<8>(0x0ULL)));
    testdout_b[2].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdout_4() {
    const vl::logic<1> v_ = vl::ror_(rd_testdouts_3()) | (vl::rd(testcode_x).bit(3) & vl::eq(vl::rd(testcode), vl::logic<8>(0x0ULL)));
    testdout_b[3].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdout_5() {
    const vl::logic<1> v_ = vl::ror_(rd_testdouts_4());
    testdout_b[4].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdout_6() {
    const vl::logic<1> v_ = vl::ror_(rd_testdouts_5());
    testdout_b[5].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdout_7() {
    const vl::logic<1> v_ = vl::ror_(rd_testdouts_6());
    testdout_b[6].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdout_8() {
    const vl::logic<1> v_ = vl::ror_(rd_testdouts_7());
    testdout_b[7].write(v_.bit(0).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_bypass_lane0() {
    vl::wr(lprxponcd_bypass_lane0, vl::rd(lprxponcd_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_target_state_bist() {
    vl::wr(target_state_bist, vl::rd(target_state_bist_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_bypass_lane2() {
    vl::wr(hsrxenterm_bypass_lane2, vl::rd(hsrxenterm_bypass_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_bypass_clklane() {
    vl::wr(lprxponulp_bypass_clklane, vl::rd(lprxponulp_bypass_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_clk_en_lanes_bypass() {
    vl::wr(clk_en_lanes_bypass, vl::rd(clk_en_lanes_bypass_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_des_en_lane3() {
    vl::wr(rx_des_en_lane3, vl::rd(rx_des_en_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxenpulldwn_bypass_lane0() {
    vl::wr(lptxenpulldwn_bypass_lane0, vl::rd(lptxenpulldwn_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_ical_x4_ovr_en() {
    vl::wr(cb_ical_x4_ovr_en, vl::rd(cb_ical_x4_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_cb_pre_iref_on() {
    vl::wr(rx_cb_pre_iref_on, vl::rd(rx_cb_pre_iref_on_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_serial_clk_en_ovr_en() {
    vl::wr(rxclk_lbck_serial_clk_en_ovr_en, vl::rd(rxclk_lbck_serial_clk_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_txstarttime() {
    vl::wr(txstarttime, vl::rd(txstarttime_rw) & vl::rep<10>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_pm_start_control_ovr_en() {
    vl::wr(rx0_pm_start_control_ovr_en, vl::rd(rx0_pm_start_control_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_cal_done_ovr() {
    vl::wr(rx3_cal_done_ovr, vl::rd(rx3_cal_done_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_bypass_lane0() {
    vl::wr(hsrxenterm_bypass_lane0, vl::rd(hsrxenterm_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxthssettle_ovr_en_datalanes() {
    vl::wr(hsrxthssettle_ovr_en_datalanes, vl::rd(hsrxthssettle_ovr_en_datalanes_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_phase_data_ovr() {
    vl::wr(rx0_rxhs_ddl_phase_data_ovr, vl::rd(rx0_rxhs_ddl_phase_data_ovr_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_setr_min() {
    vl::wr(setr_min, vl::rd(setr_min_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_tune_ovr_en() {
    vl::wr(rx1_rxhs_ddl_tune_ovr_en, vl::rd(rx1_rxhs_ddl_tune_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_on_ovr_en() {
    vl::wr(cb_on_ovr_en, vl::rd(cb_on_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_m_ovr_en() {
    vl::wr(pll_m_ovr_en, vl::rd(pll_m_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_ttawait() {
    vl::wr(ttawait, vl::rd(ttawait_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_recal_lane2_ovr() {
    vl::wr(recal_lane2_ovr, vl::rd(recal_lane2_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_aligner_dk_cnf() {
    vl::wr(aligner_dk_cnf, vl::rd(aligner_dk_cnf_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lb_prog_if() {
    vl::wr(rxclk_lb_prog_if, vl::rd(rxclk_lb_prog_if_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_ddl_start_calibration_ovr() {
    vl::wr(rx2_ddl_start_calibration_ovr, vl::rd(rx2_ddl_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_tune_en_if() {
    vl::wr(rx2_rxhs_ddl_tune_en_if, vl::rd(rx2_rxhs_ddl_tune_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_tclk_miss_div2_ovr() {
    vl::wr(tclk_miss_div2_ovr, vl::rd(tclk_miss_div2_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_ddl_finished_ovr() {
    vl::wr(rx1_ddl_finished_ovr, vl::rd(rx1_ddl_finished_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_txlp_eniocont_if() {
    vl::wr(rx0_txlp_eniocont_if, vl::rd(rx0_txlp_eniocont_if_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_data_eq_if() {
    vl::wr(rx3_rxhs_data_eq_if, vl::rd(rx3_rxhs_data_eq_if_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_clk_from_long_channel_if() {
    vl::wr(rx1_rxhs_clk_from_long_channel_if, vl::rd(rx1_rxhs_clk_from_long_channel_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_osc_pon_ovr_en() {
    vl::wr(rxclk_rxhs_osc_pon_ovr_en, vl::rd(rxclk_rxhs_osc_pon_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_feed_int_clk_ovr_en() {
    vl::wr(rx0_rxhs_feed_int_clk_ovr_en, vl::rd(rx0_rxhs_feed_int_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_pm_man_error_latch() {
    vl::wr(rx3_pm_man_error_latch, vl::rd(rx3_pm_man_error_latch_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_bypass_lane1() {
    vl::wr(hsrx_en_bypass_lane1, vl::rd(hsrx_en_bypass_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_cb_lb_setrx_if() {
    vl::wr(rx_cb_lb_setrx_if, vl::rd(rx_cb_lb_setrx_if_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_polarity_lane2() {
    vl::wr(polarity_lane2, vl::rd(polarity_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_atb_on_ovr() {
    vl::wr(cb_atb_on_ovr, vl::rd(cb_atb_on_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lb_sel_ovr_en() {
    vl::wr(rxclk_lb_sel_ovr_en, vl::rd(rxclk_lb_sel_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bypass_skew_machine() {
    vl::wr(bypass_skew_machine, vl::rd(bypass_skew_machine_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_data_retiming_out_swap_sel() {
    vl::wr(rx1_data_retiming_out_swap_sel, vl::rd(rx1_data_retiming_out_swap_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_phase_change_p_ovr_en() {
    vl::wr(rx2_rxhs_ddl_phase_change_p_ovr_en, vl::rd(rx2_rxhs_ddl_phase_change_p_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pg_data_swap() {
    vl::wr(pg_data_swap, vl::rd(pg_data_swap_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_recal_lane1_ovr() {
    vl::wr(recal_lane1_ovr, vl::rd(recal_lane1_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_lane2() {
    vl::wr(bintpon_lane2, vl::rd(bintpon_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_pm_start_control_ovr_en() {
    vl::wr(rx1_pm_start_control_ovr_en, vl::rd(rx1_pm_start_control_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_clksel_en_ovr() {
    vl::wr(pll_clksel_en_ovr, vl::rd(pll_clksel_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_gmode_if_ovr() {
    vl::wr(rx_rxhs_gmode_if_ovr, vl::rd(rx_rxhs_gmode_if_ovr_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_ddl_start_calibration_ovr() {
    vl::wr(rx3_ddl_start_calibration_ovr, vl::rd(rx3_ddl_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_des_en_lane1() {
    vl::wr(rx_des_en_lane1, vl::rd(rx_des_en_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_txlp_prog() {
    vl::wr(txlp_prog, vl::rd(txlp_prog_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_cal_done_ovr_en() {
    vl::wr(rx2_cal_done_ovr_en, vl::rd(rx2_cal_done_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_force_lock() {
    vl::wr(pll_force_lock, vl::rd(pll_force_lock_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_data_offset_cal_en_ovr() {
    vl::wr(rx0_rxhs_data_offset_cal_en_ovr, vl::rd(rx0_rxhs_data_offset_cal_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_vco_cntrl_ovr() {
    vl::wr(pll_vco_cntrl_ovr, vl::rd(pll_vco_cntrl_ovr_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_pm_start_control_ovr() {
    vl::wr(rx1_pm_start_control_ovr, vl::rd(rx1_pm_start_control_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_compatibility_mode_ovr_en() {
    vl::wr(rx_rxhs_compatibility_mode_ovr_en, vl::rd(rx_rxhs_compatibility_mode_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_del_datalane_ovr_en() {
    vl::wr(del_datalane_ovr_en, vl::rd(del_datalane_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_pm_start_control_ovr() {
    vl::wr(rx3_pm_start_control_ovr, vl::rd(rx3_pm_start_control_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_phase_data_ovr_en() {
    vl::wr(rx3_rxhs_ddl_phase_data_ovr_en, vl::rd(rx3_rxhs_ddl_phase_data_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_txlp_sr_bypass_if() {
    vl::wr(rx0_txlp_sr_bypass_if, vl::rd(rx0_txlp_sr_bypass_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bist_run_lane1() {
    vl::wr(bist_run_lane1, vl::rd(bist_run_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_bypass_lane3() {
    vl::wr(lprxponlp_bypass_lane3, vl::rd(lprxponlp_bypass_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_force_state_bist() {
    vl::wr(force_state_bist, vl::rd(force_state_bist_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_bypass_lane1() {
    vl::wr(bintpon_bypass_lane1, vl::rd(bintpon_bypass_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_ddr_clk_en_if() {
    vl::wr(rxclk_rxhs_ddr_clk_en_if, vl::rd(rxclk_rxhs_ddr_clk_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_norexttune_done_ovr() {
    vl::wr(norexttune_done_ovr, vl::rd(norexttune_done_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_recal_lane0_ovr() {
    vl::wr(recal_lane0_ovr, vl::rd(recal_lane0_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_pwron_ovr_en() {
    vl::wr(pll_pwron_ovr_en, vl::rd(pll_pwron_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_bypass_lane0() {
    vl::wr(lprxponlp_bypass_lane0, vl::rd(lprxponlp_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bist_run_lane0() {
    vl::wr(bist_run_lane0, vl::rd(bist_run_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_twakeupulpm() {
    vl::wr(twakeupulpm, vl::rd(twakeupulpm_rw) & vl::rep<10>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_pm_start_control_ovr() {
    vl::wr(rx0_pm_start_control_ovr, vl::rd(rx0_pm_start_control_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_serial_clk_en_ovr() {
    vl::wr(rxclk_lbck_serial_clk_en_ovr, vl::rd(rxclk_lbck_serial_clk_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_phase_change_p_ovr() {
    vl::wr(rx3_rxhs_ddl_phase_change_p_ovr, vl::rd(rx3_rxhs_ddl_phase_change_p_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_pm_start_control_ovr_en() {
    vl::wr(rx2_pm_start_control_ovr_en, vl::rd(rx2_pm_start_control_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_lock_ovr() {
    vl::wr(pll_lock_ovr, vl::rd(pll_lock_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_deskew_polarity() {
    vl::wr(deskew_polarity, vl::rd(deskew_polarity_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_ttasure() {
    vl::wr(ttasure, vl::rd(ttasure_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxpontime() {
    vl::wr(rxpontime, vl::rd(rxpontime_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pg_usert_pat() {
    vl::wr(pg_usert_pat, vl::rd(pg_usert_pat_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_lpibias_en_ovr() {
    vl::wr(cb_lpibias_en_ovr, vl::rd(cb_lpibias_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_div_en_ovr() {
    vl::wr(rxclk_lbck_div_en_ovr, vl::rd(rxclk_lbck_div_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_serial_en_ovr() {
    vl::wr(rxclk_lbck_serial_en_ovr, vl::rd(rxclk_lbck_serial_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_bypass_lane3() {
    vl::wr(hsrxenterm_bypass_lane3, vl::rd(hsrxenterm_bypass_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_atb_lptx1200_on_lane0() {
    vl::wr(atb_lptx1200_on_lane0, vl::rd(atb_lptx1200_on_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_gmode_if_ovr_en() {
    vl::wr(rx_rxhs_gmode_if_ovr_en, vl::rd(rx_rxhs_gmode_if_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_phase_data_ovr() {
    vl::wr(rx3_rxhs_ddl_phase_data_ovr, vl::rd(rx3_rxhs_ddl_phase_data_ovr_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_gear_shift_ovr_en() {
    vl::wr(pll_gear_shift_ovr_en, vl::rd(pll_gear_shift_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_prog_ovr_en() {
    vl::wr(rx_rxhs_prog_ovr_en, vl::rd(rx_rxhs_prog_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_repeatcal() {
    vl::wr(rx1_rxhs_repeatcal, vl::rd(rx1_rxhs_repeatcal_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_sel_vreflptx_prog() {
    vl::wr(cb_sel_vreflptx_prog, vl::rd(cb_sel_vreflptx_prog_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_atb_sel_lane3() {
    vl::wr(atb_sel_lane3, vl::rd(atb_sel_lane3_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_feed_int_clk_ovr() {
    vl::wr(rx2_rxhs_feed_int_clk_ovr, vl::rd(rx2_rxhs_feed_int_clk_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_lane2() {
    vl::wr(lprxponulp_lane2, vl::rd(lprxponulp_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_lane2() {
    vl::wr(lprxponcd_lane2, vl::rd(lprxponcd_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_bypass_lane3() {
    vl::wr(lprxponcd_bypass_lane3, vl::rd(lprxponcd_bypass_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_rx_en200mode_if() {
    vl::wr(rxclk_rxhs_rx_en200mode_if, vl::rd(rxclk_rxhs_rx_en200mode_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_clk_from_long_channel_if() {
    vl::wr(rx2_rxhs_clk_from_long_channel_if, vl::rd(rx2_rxhs_clk_from_long_channel_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_serial_clk_en_ovr_en() {
    vl::wr(rxclk_lbdata_serial_clk_en_ovr_en, vl::rd(rxclk_lbdata_serial_clk_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxtclkmisspd_ovr() {
    vl::wr(hsrxtclkmisspd_ovr, vl::rd(hsrxtclkmisspd_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_if_lane3_tester() {
    vl::wr(hsrx_en_if_lane3_tester, vl::rd(hsrx_en_if_lane3_tester_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_data_ovr() {
    vl::wr(rx0_rxhs_data_ovr, vl::rd(rx0_rxhs_data_ovr_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_start_calibration_ovr() {
    vl::wr(rx0_start_calibration_ovr, vl::rd(rx0_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_atb_comp_clk_ovr_en() {
    vl::wr(cb_atb_comp_clk_ovr_en, vl::rd(cb_atb_comp_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_serial_en_ovr_en() {
    vl::wr(rxclk_lbck_serial_en_ovr_en, vl::rd(rxclk_lbck_serial_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_deskew_numedges_update() {
    vl::wr(deskew_numedges_update, vl::rd(deskew_numedges_update_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_des_en_bypass_lane3() {
    vl::wr(rx_des_en_bypass_lane3, vl::rd(rx_des_en_bypass_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_th3() {
    vl::wr(pll_th3, vl::rd(pll_th3_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_data_eq_if() {
    vl::wr(rx0_rxhs_data_eq_if, vl::rd(rx0_rxhs_data_eq_if_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_repeatcal() {
    vl::wr(rx0_rxhs_repeatcal, vl::rd(rx0_rxhs_repeatcal_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_setr_max() {
    vl::wr(setr_max, vl::rd(setr_max_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_feed_int_clk_ovr() {
    vl::wr(rxclk_rxhs_feed_int_clk_ovr, vl::rd(rxclk_rxhs_feed_int_clk_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_counter_for_des_en_config_if() {
    vl::wr(counter_for_des_en_config_if, vl::rd(counter_for_des_en_config_if_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pg_mode_r() {
    vl::wr(pg_mode_r, vl::rd(pg_mode_r_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_if_lane3() {
    vl::wr(hsrxenterm_if_lane3, vl::rd(hsrxenterm_if_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_lb_prog_if() {
    vl::wr(rx0_lb_prog_if, vl::rd(rx0_lb_prog_if_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_tune_en_if() {
    vl::wr(rx3_rxhs_ddl_tune_en_if, vl::rd(rx3_rxhs_ddl_tune_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_bypass_lane2() {
    vl::wr(lprxponulp_bypass_lane2, vl::rd(lprxponulp_bypass_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_ddl_start_calibration_ovr_en() {
    vl::wr(rx2_ddl_start_calibration_ovr_en, vl::rd(rx2_ddl_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_start_calibration_ovr_en() {
    vl::wr(rx1_start_calibration_ovr_en, vl::rd(rx1_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lb_pll_bustie_if() {
    vl::wr(lb_pll_bustie_if, vl::rd(lb_pll_bustie_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_ddl_finished_ovr_en() {
    vl::wr(rx3_ddl_finished_ovr_en, vl::rd(rx3_ddl_finished_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_pull_long_channel_if() {
    vl::wr(rxclk_rxhs_pull_long_channel_if, vl::rd(rxclk_rxhs_pull_long_channel_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_if_lane0_tester() {
    vl::wr(hsrx_en_if_lane0_tester, vl::rd(hsrx_en_if_lane0_tester_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_ddl_finished_ovr() {
    vl::wr(rx0_ddl_finished_ovr, vl::rd(rx0_ddl_finished_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_lockdet_mode() {
    vl::wr(pll_lockdet_mode, vl::rd(pll_lockdet_mode_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_ddl_start_calibration_ovr_en() {
    vl::wr(rx1_ddl_start_calibration_ovr_en, vl::rd(rx1_ddl_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_feed_int_clk_ovr_en() {
    vl::wr(rxclk_rxhs_feed_int_clk_ovr_en, vl::rd(rxclk_rxhs_feed_int_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_lb_res_cm_en_if() {
    vl::wr(rx0_lb_res_cm_en_if, vl::rd(rx0_lb_res_cm_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_div_en_ovr_en() {
    vl::wr(rxclk_lbck_div_en_ovr_en, vl::rd(rxclk_lbck_div_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_pm_start_control_ovr() {
    vl::wr(rx2_pm_start_control_ovr, vl::rd(rx2_pm_start_control_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_data_ovr_en() {
    vl::wr(rx1_rxhs_data_ovr_en, vl::rd(rx1_rxhs_data_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_th_delay() {
    vl::wr(pll_th_delay, vl::rd(pll_th_delay_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_div_en_ovr() {
    vl::wr(rxclk_lbdata_div_en_ovr, vl::rd(rxclk_lbdata_div_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsfreqrange_ovr_en() {
    vl::wr(hsfreqrange_ovr_en, vl::rd(hsfreqrange_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_phase_clk_ovr() {
    vl::wr(rx2_rxhs_ddl_phase_clk_ovr, vl::rd(rx2_rxhs_ddl_phase_clk_ovr_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_pm_start_control_ovr_en() {
    vl::wr(rx3_pm_start_control_ovr_en, vl::rd(rx3_pm_start_control_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_if_lane3() {
    vl::wr(hsrxpon_if_lane3, vl::rd(hsrxpon_if_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_cal_done_ovr() {
    vl::wr(rx2_cal_done_ovr, vl::rd(rx2_cal_done_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_bypass_clklane() {
    vl::wr(bintpon_bypass_clklane, vl::rd(bintpon_bypass_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_reset_ovr() {
    vl::wr(pll_reset_ovr, vl::rd(pll_reset_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_feed_int_clk_ovr_en() {
    vl::wr(rx3_rxhs_feed_int_clk_ovr_en, vl::rd(rx3_rxhs_feed_int_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_target_state() {
    vl::wr(target_state, vl::rd(target_state_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_lane1() {
    vl::wr(lprxponlp_lane1, vl::rd(lprxponlp_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_data_offset_cal_ovr() {
    vl::wr(rx1_rxhs_data_offset_cal_ovr, vl::rd(rx1_rxhs_data_offset_cal_ovr_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_startup_fsm_state() {
    vl::wr(pll_startup_fsm_state, vl::rd(pll_startup_fsm_state_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cmp_polarity() {
    vl::wr(cmp_polarity, vl::rd(cmp_polarity_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bist_run_lane3() {
    vl::wr(bist_run_lane3, vl::rd(bist_run_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_pm_sample_error_counter() {
    vl::wr(rx3_pm_sample_error_counter, vl::rd(rx3_pm_sample_error_counter_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_serial_en_ovr_en() {
    vl::wr(rxclk_lbdata_serial_en_ovr_en, vl::rd(rxclk_lbdata_serial_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxena_bypass_lane0() {
    vl::wr(lptxena_bypass_lane0, vl::rd(lptxena_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_bypass_lane1() {
    vl::wr(hsrxenterm_bypass_lane1, vl::rd(hsrxenterm_bypass_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_repeatcal() {
    vl::wr(rx2_rxhs_repeatcal, vl::rd(rx2_rxhs_repeatcal_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_lane0() {
    vl::wr(lprxponulp_lane0, vl::rd(lprxponulp_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_chop_clk_en() {
    vl::wr(cb_chop_clk_en, vl::rd(cb_chop_clk_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_del_datalane_ovr() {
    vl::wr(del_datalane_ovr, vl::rd(del_datalane_ovr_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_des_en_lane0() {
    vl::wr(rx_des_en_lane0, vl::rd(rx_des_en_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxenulp_bypass_lane0() {
    vl::wr(lptxenulp_bypass_lane0, vl::rd(lptxenulp_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_dac_prog_ovr() {
    vl::wr(cb_dac_prog_ovr, vl::rd(cb_dac_prog_ovr_rw) & vl::rep<10>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pg_data_polarity() {
    vl::wr(pg_data_polarity, vl::rd(pg_data_polarity_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_ddr_clk_div_ovr_en() {
    vl::wr(rxclk_rxhs_ddr_clk_div_ovr_en, vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_deskew_jump2steps() {
    vl::wr(deskew_jump2steps, vl::rd(deskew_jump2steps_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_feed_int_clk_ovr() {
    vl::wr(rx1_rxhs_feed_int_clk_ovr, vl::rd(rx1_rxhs_feed_int_clk_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_lane1() {
    vl::wr(bintpon_lane1, vl::rd(bintpon_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxenulp_lane0() {
    vl::wr(lptxenulp_lane0, vl::rd(lptxenulp_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_recal_lane2_ovr_en() {
    vl::wr(recal_lane2_ovr_en, vl::rd(recal_lane2_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxthssettle_datalanes_ovr() {
    vl::wr(hsrxthssettle_datalanes_ovr, vl::rd(hsrxthssettle_datalanes_ovr_rw) & vl::rep<7>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_norexttune_done_ovr_en() {
    vl::wr(norexttune_done_ovr_en, vl::rd(norexttune_done_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_phase_change_p_ovr_en() {
    vl::wr(rx1_rxhs_ddl_phase_change_p_ovr_en, vl::rd(rx1_rxhs_ddl_phase_change_p_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_burnin_cal_en_ovr() {
    vl::wr(burnin_cal_en_ovr, vl::rd(burnin_cal_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_meas_iv() {
    vl::wr(pll_meas_iv, vl::rd(pll_meas_iv_rw) & vl::rep<18>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_sel_vrefcd_lprx() {
    vl::wr(cb_sel_vrefcd_lprx, vl::rd(cb_sel_vrefcd_lprx_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_feed_int_clk_ovr_en() {
    vl::wr(rx2_rxhs_feed_int_clk_ovr_en, vl::rd(rx2_rxhs_feed_int_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_lb_res_dn_en_if() {
    vl::wr(rx1_lb_res_dn_en_if, vl::rd(rx1_lb_res_dn_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_n_ovr() {
    vl::wr(pll_n_ovr, vl::rd(pll_n_ovr_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_clklane() {
    vl::wr(lprxponlp_clklane, vl::rd(lprxponlp_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_reset_ovr_en() {
    vl::wr(pll_reset_ovr_en, vl::rd(pll_reset_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_counter_for_des_en_bypass() {
    vl::wr(counter_for_des_en_bypass, vl::rd(counter_for_des_en_bypass_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_stability_time() {
    vl::wr(stability_time, vl::rd(stability_time_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_bypass_lane3() {
    vl::wr(bintpon_bypass_lane3, vl::rd(bintpon_bypass_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_if_clklane() {
    vl::wr(hsrxpon_if_clklane, vl::rd(hsrxpon_if_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_int_cntrl() {
    vl::wr(pll_int_cntrl, vl::rd(pll_int_cntrl_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_data_ovr() {
    vl::wr(rx1_rxhs_data_ovr, vl::rd(rx1_rxhs_data_ovr_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_lane2() {
    vl::wr(lprxponlp_lane2, vl::rd(lprxponlp_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_bypass_clklane() {
    vl::wr(hsrx_en_bypass_clklane, vl::rd(hsrx_en_bypass_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_ddl_finished_ovr_en() {
    vl::wr(rx0_ddl_finished_ovr_en, vl::rd(rx0_ddl_finished_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_skew_back_comp_en_ovr_en() {
    vl::wr(skew_back_comp_en_ovr_en, vl::rd(skew_back_comp_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_bypass_clklane() {
    vl::wr(lprxponlp_bypass_clklane, vl::rd(lprxponlp_bypass_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_phase_change_p_ovr() {
    vl::wr(rx0_rxhs_ddl_phase_change_p_ovr, vl::rd(rx0_rxhs_ddl_phase_change_p_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxpon_lane0() {
    vl::wr(lptxpon_lane0, vl::rd(lptxpon_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_clklane() {
    vl::wr(lprxponcd_clklane, vl::rd(lprxponcd_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_data_ovr_en() {
    vl::wr(rx0_rxhs_data_ovr_en, vl::rd(rx0_rxhs_data_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_enable_lanes_ovr_en() {
    vl::wr(enable_lanes_ovr_en, vl::rd(enable_lanes_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_clk_en_ovr_en() {
    vl::wr(rxclk_lbdata_clk_en_ovr_en, vl::rd(rxclk_lbdata_clk_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_phase_clk_ovr_en() {
    vl::wr(rx1_rxhs_ddl_phase_clk_ovr_en, vl::rd(rx1_rxhs_ddl_phase_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_lane1() {
    vl::wr(lprxponulp_lane1, vl::rd(lprxponulp_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_deskew_numedges() {
    vl::wr(deskew_numedges, vl::rd(deskew_numedges_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_force_fsm_state() {
    vl::wr(pll_force_fsm_state, vl::rd(pll_force_fsm_state_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_pm_man_error_latch() {
    vl::wr(rx0_pm_man_error_latch, vl::rd(rx0_pm_man_error_latch_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_tune_en_if() {
    vl::wr(rx1_rxhs_ddl_tune_en_if, vl::rd(rx1_rxhs_ddl_tune_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_data_offset_cal_en_ovr() {
    vl::wr(rx1_rxhs_data_offset_cal_en_ovr, vl::rd(rx1_rxhs_data_offset_cal_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_tolerance() {
    vl::wr(tolerance, vl::rd(tolerance_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_pm_mode_r() {
    vl::wr(rx3_pm_mode_r, vl::rd(rx3_pm_mode_r_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_lb_prog_if() {
    vl::wr(rx3_lb_prog_if, vl::rd(rx3_lb_prog_if_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_osc_freq_target_if_ovr_en() {
    vl::wr(rx0_osc_freq_target_if_ovr_en, vl::rd(rx0_osc_freq_target_if_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_onpll_ovr() {
    vl::wr(onpll_ovr, vl::rd(onpll_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_phase_change_p_ovr() {
    vl::wr(rx2_rxhs_ddl_phase_change_p_ovr, vl::rd(rx2_rxhs_ddl_phase_change_p_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_bypass_lane1() {
    vl::wr(lprxponlp_bypass_lane1, vl::rd(lprxponlp_bypass_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_bypass_lane0() {
    vl::wr(lprxponulp_bypass_lane0, vl::rd(lprxponulp_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_rst_time() {
    vl::wr(pll_rst_time, vl::rd(pll_rst_time_rw) & vl::rep<9>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_if_lane2_tester() {
    vl::wr(hsrx_en_if_lane2_tester, vl::rd(hsrx_en_if_lane2_tester_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_lb_res_cm_en_if() {
    vl::wr(rx1_lb_res_cm_en_if, vl::rd(rx1_lb_res_cm_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_burnin_cal_en_ovr_en() {
    vl::wr(burnin_cal_en_ovr_en, vl::rd(burnin_cal_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_lpibias_en_ovr_en() {
    vl::wr(cb_lpibias_en_ovr_en, vl::rd(cb_lpibias_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_tune_ovr_en() {
    vl::wr(rx0_rxhs_ddl_tune_ovr_en, vl::rd(rx0_rxhs_ddl_tune_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_ddl_osc_freq_target_ovr() {
    vl::wr(ddl_osc_freq_target_ovr, vl::rd(ddl_osc_freq_target_ovr_rw) & vl::rep<12>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_serial_clk_en_ovr() {
    vl::wr(rxclk_lbdata_serial_clk_en_ovr, vl::rd(rxclk_lbdata_serial_clk_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_clkouten_right() {
    vl::wr(pll_clkouten_right, vl::rd(pll_clkouten_right_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_osc_res_en_ovr() {
    vl::wr(rxclk_rxhs_osc_res_en_ovr, vl::rd(rxclk_rxhs_osc_res_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsdirect() {
    vl::wr(hsdirect, vl::rd(hsdirect_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_ddl_start_calibration_ovr_en() {
    vl::wr(rx3_ddl_start_calibration_ovr_en, vl::rd(rx3_ddl_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_lb_res_dp_en_if() {
    vl::wr(rx1_lb_res_dp_en_if, vl::rd(rx1_lb_res_dp_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_enable_lanes_ovr() {
    vl::wr(enable_lanes_ovr, vl::rd(enable_lanes_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_bypass_lane3() {
    vl::wr(hsrxpon_bypass_lane3, vl::rd(hsrxpon_bypass_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lb_res_dn_en_if() {
    vl::wr(rxclk_lb_res_dn_en_if, vl::rd(rxclk_lb_res_dn_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsfreqrange_ovr() {
    vl::wr(hsfreqrange_ovr, vl::rd(hsfreqrange_ovr_rw) & vl::rep<7>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_des_en_lane2() {
    vl::wr(rx_des_en_lane2, vl::rd(rx_des_en_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxtclkmisspd_ovr_en() {
    vl::wr(hsrxtclkmisspd_ovr_en, vl::rd(hsrxtclkmisspd_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_ddl_offset_error() {
    vl::wr(rx_ddl_offset_error, vl::rd(rx_ddl_offset_error_rw) & vl::rep<12>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_ddl_finished_ovr() {
    vl::wr(rx2_ddl_finished_ovr, vl::rd(rx2_ddl_finished_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_bypass_lane0() {
    vl::wr(bintpon_bypass_lane0, vl::rd(bintpon_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_ttago() {
    vl::wr(ttago, vl::rd(ttago_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_deskew_legacyz_ovr_en() {
    vl::wr(deskew_legacyz_ovr_en, vl::rd(deskew_legacyz_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_osc_freq_target_if_ovr() {
    vl::wr(rx3_osc_freq_target_if_ovr, vl::rd(rx3_osc_freq_target_if_ovr_rw) & vl::rep<12>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_ddr_clk_div_ovr() {
    vl::wr(rxclk_rxhs_ddr_clk_div_ovr, vl::rd(rxclk_rxhs_ddr_clk_div_ovr_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_offset_comp_threshold() {
    vl::wr(offset_comp_threshold, vl::rd(offset_comp_threshold_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_ddl_clk_out_div() {
    vl::wr(rx_rxhs_ddl_clk_out_div, vl::rd(rx_rxhs_ddl_clk_out_div_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_atb_sel_clklane() {
    vl::wr(atb_sel_clklane, vl::rd(atb_sel_clklane_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_count_ref() {
    vl::wr(count_ref, vl::rd(count_ref_rw) & vl::rep<10>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_polarity_lane1() {
    vl::wr(polarity_lane1, vl::rd(polarity_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_if_lane0() {
    vl::wr(hsrxpon_if_lane0, vl::rd(hsrxpon_if_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_int_clk_sel() {
    vl::wr(rxclk_rxhs_int_clk_sel, vl::rd(rxclk_rxhs_int_clk_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_gear_shift_ovr() {
    vl::wr(pll_gear_shift_ovr, vl::rd(pll_gear_shift_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_data_offset_cal_ovr_en() {
    vl::wr(rx2_rxhs_data_offset_cal_ovr_en, vl::rd(rx2_rxhs_data_offset_cal_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxpon_bypass_lane0() {
    vl::wr(lptxpon_bypass_lane0, vl::rd(lptxpon_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_lb_res_dn_en_if() {
    vl::wr(rx3_lb_res_dn_en_if, vl::rd(rx3_lb_res_dn_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_cal_repl_ovr() {
    vl::wr(cb_cal_repl_ovr, vl::rd(cb_cal_repl_ovr_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_tune_ovr() {
    vl::wr(rx2_rxhs_ddl_tune_ovr, vl::rd(rx2_rxhs_ddl_tune_ovr_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_vref_mpll_reg_sel() {
    vl::wr(cb_vref_mpll_reg_sel, vl::rd(cb_vref_mpll_reg_sel_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_bypass_clklane() {
    vl::wr(hsrxpon_bypass_clklane, vl::rd(hsrxpon_bypass_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_boot_from_start() {
    vl::wr(pll_boot_from_start, vl::rd(pll_boot_from_start_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_clkouten_left() {
    vl::wr(pll_clkouten_left, vl::rd(pll_clkouten_left_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_phase_clk_ovr() {
    vl::wr(rx3_rxhs_ddl_phase_clk_ovr, vl::rd(rx3_rxhs_ddl_phase_clk_ovr_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_clk_kill_if() {
    vl::wr(rxclk_lbdata_clk_kill_if, vl::rd(rxclk_lbdata_clk_kill_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_if_lane2() {
    vl::wr(hsrxenterm_if_lane2, vl::rd(hsrxenterm_if_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_atb_sense_sel() {
    vl::wr(pll_atb_sense_sel, vl::rd(pll_atb_sense_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_tune_ovr_en() {
    vl::wr(rx3_rxhs_ddl_tune_ovr_en, vl::rd(rx3_rxhs_ddl_tune_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_deskew_legacyz_ovr() {
    vl::wr(deskew_legacyz_ovr, vl::rd(deskew_legacyz_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_bypass_clklane() {
    vl::wr(hsrxenterm_bypass_clklane, vl::rd(hsrxenterm_bypass_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_tune_en_bypass() {
    vl::wr(rx0_rxhs_ddl_tune_en_bypass, vl::rd(rx0_rxhs_ddl_tune_en_bypass_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_data_offset_cal_en_ovr_en() {
    vl::wr(rx0_rxhs_data_offset_cal_en_ovr_en, vl::rd(rx0_rxhs_data_offset_cal_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_lb_res_dn_en_if() {
    vl::wr(rx0_lb_res_dn_en_if, vl::rd(rx0_lb_res_dn_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_if_clklane() {
    vl::wr(hsrxenterm_if_clklane, vl::rd(hsrxenterm_if_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_mastermacro() {
    vl::wr(mastermacro, vl::rd(mastermacro_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_vco_cntrl_ovr_en() {
    vl::wr(pll_vco_cntrl_ovr_en, vl::rd(pll_vco_cntrl_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pipe_clean_time() {
    vl::wr(pipe_clean_time, vl::rd(pipe_clean_time_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_lane0() {
    vl::wr(bintpon_lane0, vl::rd(bintpon_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_if_lane0() {
    vl::wr(hsrxenterm_if_lane0, vl::rd(hsrxenterm_if_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_tune_en_bypass() {
    vl::wr(rx2_rxhs_ddl_tune_en_bypass, vl::rd(rx2_rxhs_ddl_tune_en_bypass_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_timebase_ovr() {
    vl::wr(timebase_ovr, vl::rd(timebase_ovr_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_ddl_finished_ovr() {
    vl::wr(rx3_ddl_finished_ovr, vl::rd(rx3_ddl_finished_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_clk_kill_if() {
    vl::wr(rxclk_lbck_clk_kill_if, vl::rd(rxclk_lbck_clk_kill_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_tafailsafez() {
    vl::wr(tafailsafez, vl::rd(tafailsafez_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_fsm_trigger_ovr() {
    vl::wr(fsm_trigger_ovr, vl::rd(fsm_trigger_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_atb_comp_clk_ovr() {
    vl::wr(cb_atb_comp_clk_ovr, vl::rd(cb_atb_comp_clk_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_bypass_clklane() {
    vl::wr(lprxponcd_bypass_clklane, vl::rd(lprxponcd_bypass_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_atb_sel_lane1() {
    vl::wr(atb_sel_lane1, vl::rd(atb_sel_lane1_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_lock_sel() {
    vl::wr(pll_lock_sel, vl::rd(pll_lock_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_recal_lane1_ovr_en() {
    vl::wr(recal_lane1_ovr_en, vl::rd(recal_lane1_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxena_lane0() {
    vl::wr(lptxena_lane0, vl::rd(lptxena_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_sel_vref_lprx() {
    vl::wr(cb_sel_vref_lprx, vl::rd(cb_sel_vref_lprx_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_deskew_overflow() {
    vl::wr(deskew_overflow, vl::rd(deskew_overflow_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_setr_typ() {
    vl::wr(setr_typ, vl::rd(setr_typ_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_if_lane2() {
    vl::wr(hsrxpon_if_lane2, vl::rd(hsrxpon_if_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_osc_freq_target_if_ovr() {
    vl::wr(rx0_osc_freq_target_if_ovr, vl::rd(rx0_osc_freq_target_if_ovr_rw) & vl::rep<12>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_skew_back_comp_en_ovr() {
    vl::wr(skew_back_comp_en_ovr, vl::rd(skew_back_comp_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_atb_on_ovr_en() {
    vl::wr(cb_atb_on_ovr_en, vl::rd(cb_atb_on_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_data_if() {
    vl::wr(rxclk_lbck_data_if, vl::rd(rxclk_lbck_data_if_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_data_offset_cal_ovr() {
    vl::wr(rx3_rxhs_data_offset_cal_ovr, vl::rd(rx3_rxhs_data_offset_cal_ovr_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_gear_shift_time() {
    vl::wr(pll_gear_shift_time, vl::rd(pll_gear_shift_time_rw) & vl::rep<9>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_clk_from_long_channel_if() {
    vl::wr(rx3_rxhs_clk_from_long_channel_if, vl::rd(rx3_rxhs_clk_from_long_channel_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_fsm_trigger_ovr_en() {
    vl::wr(fsm_trigger_ovr_en, vl::rd(fsm_trigger_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_atb_sel() {
    vl::wr(cb_atb_sel, vl::rd(cb_atb_sel_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_phase_clk_ovr_en() {
    vl::wr(rx2_rxhs_ddl_phase_clk_ovr_en, vl::rd(rx2_rxhs_ddl_phase_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_lock_det_on_ovr_en() {
    vl::wr(pll_lock_det_on_ovr_en, vl::rd(pll_lock_det_on_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_data_offset_cal_en_ovr() {
    vl::wr(rx3_rxhs_data_offset_cal_en_ovr, vl::rd(rx3_rxhs_data_offset_cal_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_cb_lb_en_rext_if() {
    vl::wr(rx_cb_lb_en_rext_if, vl::rd(rx_cb_lb_en_rext_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_bypass_lane3() {
    vl::wr(hsrx_en_bypass_lane3, vl::rd(hsrx_en_bypass_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_skew_mux_sel() {
    vl::wr(skew_mux_sel, vl::rd(skew_mux_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_ddl_start_calibration_ovr_en() {
    vl::wr(rx0_ddl_start_calibration_ovr_en, vl::rd(rx0_ddl_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_start_calibration_ovr_en() {
    vl::wr(rx3_start_calibration_ovr_en, vl::rd(rx3_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_tune_ovr_en() {
    vl::wr(rx2_rxhs_ddl_tune_ovr_en, vl::rd(rx2_rxhs_ddl_tune_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_start_calibration_ovr() {
    vl::wr(rxclk_rxhs_start_calibration_ovr, vl::rd(rxclk_rxhs_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_feed_int_clk_ovr() {
    vl::wr(rx0_rxhs_feed_int_clk_ovr, vl::rd(rx0_rxhs_feed_int_clk_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_bg_en_ovr() {
    vl::wr(cb_bg_en_ovr, vl::rd(cb_bg_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_sel_chop_clk() {
    vl::wr(cb_sel_chop_clk, vl::rd(cb_sel_chop_clk_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_start_calibration_ovr() {
    vl::wr(rxclk_start_calibration_ovr, vl::rd(rxclk_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_data_offset_cal_en_ovr_en() {
    vl::wr(rx2_rxhs_data_offset_cal_en_ovr_en, vl::rd(rx2_rxhs_data_offset_cal_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_data_retiming_out_swap_sel() {
    vl::wr(rx0_data_retiming_out_swap_sel, vl::rd(rx0_data_retiming_out_swap_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_tclk_miss_opt() {
    vl::wr(tclk_miss_opt, vl::rd(tclk_miss_opt_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_serial_en_ovr() {
    vl::wr(rxclk_lbdata_serial_en_ovr, vl::rd(rxclk_lbdata_serial_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_pwron_ovr() {
    vl::wr(pll_pwron_ovr, vl::rd(pll_pwron_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_polarity_lane0() {
    vl::wr(polarity_lane0, vl::rd(polarity_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_start_calibration_ovr() {
    vl::wr(rx1_start_calibration_ovr, vl::rd(rx1_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_lb_prog_if() {
    vl::wr(rx1_lb_prog_if, vl::rd(rx1_lb_prog_if_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_ical_x4_ovr() {
    vl::wr(cb_ical_x4_ovr, vl::rd(cb_ical_x4_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_start_calibration_ovr_en() {
    vl::wr(rxclk_start_calibration_ovr_en, vl::rd(rxclk_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_lane3() {
    vl::wr(bintpon_lane3, vl::rd(bintpon_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_data_eq_if() {
    vl::wr(rx2_rxhs_data_eq_if, vl::rd(rx2_rxhs_data_eq_if_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_compatibility_mode_ovr() {
    vl::wr(rx_rxhs_compatibility_mode_ovr, vl::rd(rx_rxhs_compatibility_mode_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_pm_sample_error_counter() {
    vl::wr(rx0_pm_sample_error_counter, vl::rd(rx0_pm_sample_error_counter_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_start_calibration_ovr_en() {
    vl::wr(rx0_start_calibration_ovr_en, vl::rd(rx0_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_th1() {
    vl::wr(pll_th1, vl::rd(pll_th1_rw) & vl::rep<10>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_cal_en_ovr() {
    vl::wr(cb_cal_en_ovr, vl::rd(cb_cal_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_clklane() {
    vl::wr(bintpon_clklane, vl::rd(bintpon_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_if_clklane_tester() {
    vl::wr(hsrx_en_if_clklane_tester, vl::rd(hsrx_en_if_clklane_tester_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_pm_mode_r() {
    vl::wr(rx1_pm_mode_r, vl::rd(rx1_pm_mode_r_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_data_ovr() {
    vl::wr(rx2_rxhs_data_ovr, vl::rd(rx2_rxhs_data_ovr_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bintpon_bypass_lane2() {
    vl::wr(bintpon_bypass_lane2, vl::rd(bintpon_bypass_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_atb_sel_lane0() {
    vl::wr(atb_sel_lane0, vl::rd(atb_sel_lane0_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_opmode() {
    vl::wr(pll_opmode, vl::rd(pll_opmode_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_osc_freq_target_if_ovr() {
    vl::wr(rx2_osc_freq_target_if_ovr, vl::rd(rx2_osc_freq_target_if_ovr_rw) & vl::rep<12>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_txdindlytime() {
    vl::wr(txdindlytime, vl::rd(txdindlytime_rw) & vl::rep<10>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_osc_freq_target_if_ovr() {
    vl::wr(rx1_osc_freq_target_if_ovr, vl::rd(rx1_osc_freq_target_if_ovr_rw) & vl::rep<12>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_lock_det_time() {
    vl::wr(pll_lock_det_time, vl::rd(pll_lock_det_time_rw) & vl::rep<9>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_clk_offset_cal_ovr() {
    vl::wr(rxclk_rxhs_clk_offset_cal_ovr, vl::rd(rxclk_rxhs_clk_offset_cal_ovr_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_data_ovr_en() {
    vl::wr(rx3_rxhs_data_ovr_en, vl::rd(rx3_rxhs_data_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_data_offset_cal_ovr() {
    vl::wr(rx2_rxhs_data_offset_cal_ovr, vl::rd(rx2_rxhs_data_offset_cal_ovr_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pg_bist_error() {
    vl::wr(pg_bist_error, vl::rd(pg_bist_error_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_en_45_ohm() {
    vl::wr(cb_en_45_ohm, vl::rd(cb_en_45_ohm_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_lb_res_cm_en_if() {
    vl::wr(rx2_lb_res_cm_en_if, vl::rd(rx2_lb_res_cm_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_phase_clk_ovr() {
    vl::wr(rx1_rxhs_ddl_phase_clk_ovr, vl::rd(rx1_rxhs_ddl_phase_clk_ovr_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_lane0() {
    vl::wr(lprxponcd_lane0, vl::rd(lprxponcd_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_phase_data_ovr_en() {
    vl::wr(rx0_rxhs_ddl_phase_data_ovr_en, vl::rd(rx0_rxhs_ddl_phase_data_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_lb_res_dp_en_if() {
    vl::wr(rx3_lb_res_dp_en_if, vl::rd(rx3_lb_res_dp_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_pm_man_error_latch() {
    vl::wr(rx1_pm_man_error_latch, vl::rd(rx1_pm_man_error_latch_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_cal_repl_ovr_en() {
    vl::wr(cb_cal_repl_ovr_en, vl::rd(cb_cal_repl_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_ddl_finished_ovr_en() {
    vl::wr(rx2_ddl_finished_ovr_en, vl::rd(rx2_ddl_finished_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_bypass_lane0() {
    vl::wr(hsrx_en_bypass_lane0, vl::rd(hsrx_en_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_ddl_finished_ovr_en() {
    vl::wr(rx1_ddl_finished_ovr_en, vl::rd(rx1_ddl_finished_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_tune_en_if() {
    vl::wr(rx0_rxhs_ddl_tune_en_if, vl::rd(rx0_rxhs_ddl_tune_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_data_eq_if() {
    vl::wr(rx1_rxhs_data_eq_if, vl::rd(rx1_rxhs_data_eq_if_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_data_ovr_en() {
    vl::wr(rx2_rxhs_data_ovr_en, vl::rd(rx2_rxhs_data_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_phase_data_ovr_en() {
    vl::wr(rx1_rxhs_ddl_phase_data_ovr_en, vl::rd(rx1_rxhs_ddl_phase_data_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_des_en_bypass_lane1() {
    vl::wr(rx_des_en_bypass_lane1, vl::rd(rx_des_en_bypass_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_cal_done_ovr() {
    vl::wr(rx0_cal_done_ovr, vl::rd(rx0_cal_done_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lb_res_dp_en_if() {
    vl::wr(rxclk_lb_res_dp_en_if, vl::rd(rxclk_lb_res_dp_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_tclk_miss_div2_ovr_en() {
    vl::wr(tclk_miss_div2_ovr_en, vl::rd(tclk_miss_div2_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_polarity_lane3() {
    vl::wr(polarity_lane3, vl::rd(polarity_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_cal_done_ovr_en() {
    vl::wr(rx0_cal_done_ovr_en, vl::rd(rx0_cal_done_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_setr_ovr_en() {
    vl::wr(rx_rxhs_setr_ovr_en, vl::rd(rx_rxhs_setr_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_polarity_clklane() {
    vl::wr(polarity_clklane, vl::rd(polarity_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_number_of_ready_count() {
    vl::wr(number_of_ready_count, vl::rd(number_of_ready_count_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_data_ovr() {
    vl::wr(rx3_rxhs_data_ovr, vl::rd(rx3_rxhs_data_ovr_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_bypass_lane0() {
    vl::wr(hsrxpon_bypass_lane0, vl::rd(hsrxpon_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_div_en_ovr_en() {
    vl::wr(rxclk_lbdata_div_en_ovr_en, vl::rd(rxclk_lbdata_div_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bypass_ddltunning_machine() {
    vl::wr(bypass_ddltunning_machine, vl::rd(bypass_ddltunning_machine_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lb_res_cm_en_if() {
    vl::wr(rxclk_lb_res_cm_en_if, vl::rd(rxclk_lb_res_cm_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_lb_prog_if() {
    vl::wr(rx2_lb_prog_if, vl::rd(rx2_lb_prog_if_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_lock_ovr_en() {
    vl::wr(pll_lock_ovr_en, vl::rd(pll_lock_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_data_retiming_out_swap_sel() {
    vl::wr(rx3_data_retiming_out_swap_sel, vl::rd(rx3_data_retiming_out_swap_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxenterm_if_lane1() {
    vl::wr(hsrxenterm_if_lane1, vl::rd(hsrxenterm_if_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_clk_from_long_channel_if() {
    vl::wr(rx0_rxhs_clk_from_long_channel_if, vl::rd(rx0_rxhs_clk_from_long_channel_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_caldone_ovr() {
    vl::wr(rxclk_caldone_ovr, vl::rd(rxclk_caldone_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_data_offset_cal_en_ovr_en() {
    vl::wr(rx3_rxhs_data_offset_cal_en_ovr_en, vl::rd(rx3_rxhs_data_offset_cal_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_recal_lane3_ovr_en() {
    vl::wr(recal_lane3_ovr_en, vl::rd(recal_lane3_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_lb_res_dp_en_if() {
    vl::wr(rx2_lb_res_dp_en_if, vl::rd(rx2_lb_res_dp_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_pm_mode_r() {
    vl::wr(rx0_pm_mode_r, vl::rd(rx0_pm_mode_r_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_start_calibration_ovr() {
    vl::wr(rx2_start_calibration_ovr, vl::rd(rx2_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_sel_v400_prog() {
    vl::wr(cb_sel_v400_prog, vl::rd(cb_sel_v400_prog_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_if_lane1() {
    vl::wr(hsrxpon_if_lane1, vl::rd(hsrxpon_if_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bist_run_lane2() {
    vl::wr(bist_run_lane2, vl::rd(bist_run_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_phase_change_p_ovr_en() {
    vl::wr(rx3_rxhs_ddl_phase_change_p_ovr_en, vl::rd(rx3_rxhs_ddl_phase_change_p_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_noalign_error() {
    vl::wr(noalign_error, vl::rd(noalign_error_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_clklane() {
    vl::wr(lprxponulp_clklane, vl::rd(lprxponulp_clklane_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_osc_freq_target_if_ovr_en() {
    vl::wr(rx2_osc_freq_target_if_ovr_en, vl::rd(rx2_osc_freq_target_if_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_skew_mux_run() {
    vl::wr(skew_mux_run, vl::rd(skew_mux_run_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_pm_man_error_latch() {
    vl::wr(rx2_pm_man_error_latch, vl::rd(rx2_pm_man_error_latch_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lb_sel_ovr() {
    vl::wr(rxclk_lb_sel_ovr, vl::rd(rxclk_lb_sel_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_bypass_lane2() {
    vl::wr(lprxponlp_bypass_lane2, vl::rd(lprxponlp_bypass_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_onpll_ovr_en() {
    vl::wr(onpll_ovr_en, vl::rd(onpll_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_clk_en_ovr_en() {
    vl::wr(rxclk_lbck_clk_en_ovr_en, vl::rd(rxclk_lbck_clk_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_ddl_osc_freq_target_ovr_en() {
    vl::wr(ddl_osc_freq_target_ovr_en, vl::rd(ddl_osc_freq_target_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_data_retiming_out_swap_sel() {
    vl::wr(rx2_data_retiming_out_swap_sel, vl::rd(rx2_data_retiming_out_swap_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_bypass_lane3() {
    vl::wr(lprxponulp_bypass_lane3, vl::rd(lprxponulp_bypass_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_pm_sample_error_counter() {
    vl::wr(rx2_pm_sample_error_counter, vl::rd(rx2_pm_sample_error_counter_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bypass_offset_machine() {
    vl::wr(bypass_offset_machine, vl::rd(bypass_offset_machine_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_prop_cntrl() {
    vl::wr(pll_prop_cntrl, vl::rd(pll_prop_cntrl_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_cb_ibias_en_if() {
    vl::wr(rx_cb_ibias_en_if, vl::rd(rx_cb_ibias_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_lane1() {
    vl::wr(lprxponcd_lane1, vl::rd(lprxponcd_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_m_ovr() {
    vl::wr(pll_m_ovr, vl::rd(pll_m_ovr_rw) & vl::rep<10>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_recal_lane0_ovr_en() {
    vl::wr(recal_lane0_ovr_en, vl::rd(recal_lane0_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_atb_ext_con() {
    vl::wr(cb_atb_ext_con, vl::rd(cb_atb_ext_con_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_cal_done_ovr_en() {
    vl::wr(rx3_cal_done_ovr_en, vl::rd(rx3_cal_done_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_lock_det_on_ovr() {
    vl::wr(pll_lock_det_on_ovr, vl::rd(pll_lock_det_on_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_adc_done_clear() {
    vl::wr(adc_done_clear, vl::rd(adc_done_clear_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_cal_done_ovr_en() {
    vl::wr(rx1_cal_done_ovr_en, vl::rd(rx1_cal_done_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_bypass_lane2() {
    vl::wr(hsrxpon_bypass_lane2, vl::rd(hsrxpon_bypass_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_phase_clk_ovr_en() {
    vl::wr(rx0_rxhs_ddl_phase_clk_ovr_en, vl::rd(rx0_rxhs_ddl_phase_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_force_state() {
    vl::wr(force_state, vl::rd(force_state_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_clk_offset_cal_ovr_en() {
    vl::wr(rxclk_rxhs_clk_offset_cal_ovr_en, vl::rd(rxclk_rxhs_clk_offset_cal_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_phase_data_ovr_en() {
    vl::wr(rx2_rxhs_ddl_phase_data_ovr_en, vl::rd(rx2_rxhs_ddl_phase_data_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_if_lane1_tester() {
    vl::wr(hsrx_en_if_lane1_tester, vl::rd(hsrx_en_if_lane1_tester_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_ddr_clk_en_bypass() {
    vl::wr(rxclk_rxhs_ddr_clk_en_bypass, vl::rd(rxclk_rxhs_ddr_clk_en_bypass_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_timeinitbg() {
    vl::wr(timeinitbg, vl::rd(timeinitbg_rw) & vl::rep<10>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_caldone_ovr_en() {
    vl::wr(rxclk_caldone_ovr_en, vl::rd(rxclk_caldone_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_pm_sample_error_counter() {
    vl::wr(rx1_pm_sample_error_counter, vl::rd(rx1_pm_sample_error_counter_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_bypass_lane1() {
    vl::wr(lprxponcd_bypass_lane1, vl::rd(lprxponcd_bypass_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_ddr_clk_sel_if() {
    vl::wr(rxclk_rxhs_ddr_clk_sel_if, vl::rd(rxclk_rxhs_ddr_clk_sel_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxlp_bias_prog() {
    vl::wr(rx_rxlp_bias_prog, vl::rd(rx_rxlp_bias_prog_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_lane3() {
    vl::wr(lprxponulp_lane3, vl::rd(lprxponulp_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_repeatcal() {
    vl::wr(rx3_rxhs_repeatcal, vl::rd(rx3_rxhs_repeatcal_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_lane3() {
    vl::wr(lprxponcd_lane3, vl::rd(lprxponcd_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_tune_ovr() {
    vl::wr(rx1_rxhs_ddl_tune_ovr, vl::rd(rx1_rxhs_ddl_tune_ovr_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_clk_en_lanes_tester() {
    vl::wr(clk_en_lanes_tester, vl::rd(clk_en_lanes_tester_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_tstplldig() {
    vl::wr(pll_tstplldig, vl::rd(pll_tstplldig_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_phase_clk_ovr_en() {
    vl::wr(rx3_rxhs_ddl_phase_clk_ovr_en, vl::rd(rx3_rxhs_ddl_phase_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_data_offset_cal_en_ovr_en() {
    vl::wr(rx1_rxhs_data_offset_cal_en_ovr_en, vl::rd(rx1_rxhs_data_offset_cal_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_data_offset_cal_ovr() {
    vl::wr(rx0_rxhs_data_offset_cal_ovr, vl::rd(rx0_rxhs_data_offset_cal_ovr_rw) & vl::rep<6>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_gmp_cntrl() {
    vl::wr(pll_gmp_cntrl, vl::rd(pll_gmp_cntrl_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_noext_burnin_res_cal() {
    vl::wr(noext_burnin_res_cal, vl::rd(noext_burnin_res_cal_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_th2() {
    vl::wr(pll_th2, vl::rd(pll_th2_rw) & vl::rep<8>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_atb_sel_lane2() {
    vl::wr(atb_sel_lane2, vl::rd(atb_sel_lane2_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_tune_ovr() {
    vl::wr(rx3_rxhs_ddl_tune_ovr, vl::rd(rx3_rxhs_ddl_tune_ovr_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_ddl_phase_data_ovr() {
    vl::wr(rx2_rxhs_ddl_phase_data_ovr, vl::rd(rx2_rxhs_ddl_phase_data_ovr_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_phase_data_ovr() {
    vl::wr(rx1_rxhs_ddl_phase_data_ovr, vl::rd(rx1_rxhs_ddl_phase_data_ovr_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_cal_en_ovr_en() {
    vl::wr(cb_cal_en_ovr_en, vl::rd(cb_cal_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_timebase_ovr_en() {
    vl::wr(timebase_ovr_en, vl::rd(timebase_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_osc_freq_target_if_ovr_en() {
    vl::wr(rx3_osc_freq_target_if_ovr_en, vl::rd(rx3_osc_freq_target_if_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_lb_res_cm_en_if() {
    vl::wr(rx3_lb_res_cm_en_if, vl::rd(rx3_lb_res_cm_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrx_en_bypass_lane2() {
    vl::wr(hsrx_en_bypass_lane2, vl::rd(hsrx_en_bypass_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_mpll_prog() {
    vl::wr(pll_mpll_prog, vl::rd(pll_mpll_prog_rw) & vl::rep<17>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponulp_bypass_lane1() {
    vl::wr(lprxponulp_bypass_lane1, vl::rd(lprxponulp_bypass_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_lane0() {
    vl::wr(lprxponlp_lane0, vl::rd(lprxponlp_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_cpbias_cntrl() {
    vl::wr(pll_cpbias_cntrl, vl::rd(pll_cpbias_cntrl_rw) & vl::rep<7>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_prog_ovr() {
    vl::wr(rx_rxhs_prog_ovr, vl::rd(rx_rxhs_prog_ovr_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_bist_manual() {
    vl::wr(bist_manual, vl::rd(bist_manual_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_lb_res_dn_en_if() {
    vl::wr(rx2_lb_res_dn_en_if, vl::rd(rx2_lb_res_dn_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_adc_en() {
    vl::wr(adc_en, vl::rd(adc_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_feed_int_clk_ovr() {
    vl::wr(rx3_rxhs_feed_int_clk_ovr, vl::rd(rx3_rxhs_feed_int_clk_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponlp_lane3() {
    vl::wr(lprxponlp_lane3, vl::rd(lprxponlp_lane3_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_pm_mode_r() {
    vl::wr(rx2_pm_mode_r, vl::rd(rx2_pm_mode_r_rw) & vl::rep<4>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_osc_freq_target_if_ovr_en() {
    vl::wr(rx1_osc_freq_target_if_ovr_en, vl::rd(rx1_osc_freq_target_if_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxdin_bypass_lane0() {
    vl::wr(lptxdin_bypass_lane0, vl::rd(lptxdin_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_phase_change_p_ovr_en() {
    vl::wr(rx0_rxhs_ddl_phase_change_p_ovr_en, vl::rd(rx0_rxhs_ddl_phase_change_p_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxenpulldwn_lane0() {
    vl::wr(lptxenpulldwn_lane0, vl::rd(lptxenpulldwn_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_clk_to_long_channel_if() {
    vl::wr(rxclk_rxhs_clk_to_long_channel_if, vl::rd(rxclk_rxhs_clk_to_long_channel_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbck_clk_en_ovr() {
    vl::wr(rxclk_lbck_clk_en_ovr, vl::rd(rxclk_lbck_clk_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_atb_vbe_sel() {
    vl::wr(cb_atb_vbe_sel, vl::rd(cb_atb_vbe_sel_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_data_offset_cal_ovr_en() {
    vl::wr(rx3_rxhs_data_offset_cal_ovr_en, vl::rd(rx3_rxhs_data_offset_cal_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pg_reg_err_r() {
    vl::wr(pg_reg_err_r, vl::rd(pg_reg_err_r_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_deskew_latency() {
    vl::wr(deskew_latency, vl::rd(deskew_latency_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_ddl_start_calibration_ovr() {
    vl::wr(rx0_ddl_start_calibration_ovr, vl::rd(rx0_ddl_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_comp_after_strobes() {
    vl::wr(comp_after_strobes, vl::rd(comp_after_strobes_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_des_en_bypass_lane2() {
    vl::wr(rx_des_en_bypass_lane2, vl::rd(rx_des_en_bypass_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_clksel_en_ovr_en() {
    vl::wr(pll_clksel_en_ovr_en, vl::rd(pll_clksel_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_ddl_start_calibration_ovr() {
    vl::wr(rx1_ddl_start_calibration_ovr, vl::rd(rx1_ddl_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_phase_change_p_ovr() {
    vl::wr(rx1_rxhs_ddl_phase_change_p_ovr, vl::rd(rx1_rxhs_ddl_phase_change_p_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_feed_int_clk_ovr_en() {
    vl::wr(rx1_rxhs_feed_int_clk_ovr_en, vl::rd(rx1_rxhs_feed_int_clk_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_data_offset_cal_ovr_en() {
    vl::wr(rx1_rxhs_data_offset_cal_ovr_en, vl::rd(rx1_rxhs_data_offset_cal_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_pll_n_ovr_en() {
    vl::wr(pll_n_ovr_en, vl::rd(pll_n_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_des_en_bypass_lane0() {
    vl::wr(rx_des_en_bypass_lane0, vl::rd(rx_des_en_bypass_lane0_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_hsrxpon_bypass_lane1() {
    vl::wr(hsrxpon_bypass_lane1, vl::rd(hsrxpon_bypass_lane1_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_rxhs_ddl_tune_en_bypass() {
    vl::wr(rx3_rxhs_ddl_tune_en_bypass, vl::rd(rx3_rxhs_ddl_tune_en_bypass_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_rxhs_data_offset_cal_en_ovr() {
    vl::wr(rx2_rxhs_data_offset_cal_en_ovr, vl::rd(rx2_rxhs_data_offset_cal_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lptxdin_lane0() {
    vl::wr(lptxdin_lane0, vl::rd(lptxdin_lane0_rw) & vl::rep<2>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_cal_done_ovr() {
    vl::wr(rx1_cal_done_ovr, vl::rd(rx1_cal_done_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_data_offset_cal_ovr_en() {
    vl::wr(rx0_rxhs_data_offset_cal_ovr_en, vl::rd(rx0_rxhs_data_offset_cal_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_recal_lane3_ovr() {
    vl::wr(recal_lane3_ovr, vl::rd(recal_lane3_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_phase_clk_ovr() {
    vl::wr(rx0_rxhs_ddl_phase_clk_ovr, vl::rd(rx0_rxhs_ddl_phase_clk_ovr_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_lprxponcd_bypass_lane2() {
    vl::wr(lprxponcd_bypass_lane2, vl::rd(lprxponcd_bypass_lane2_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_bg_en_ovr_en() {
    vl::wr(cb_bg_en_ovr_en, vl::rd(cb_bg_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_osc_pon_ovr() {
    vl::wr(rxclk_rxhs_osc_pon_ovr, vl::rd(rxclk_rxhs_osc_pon_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_start_calibration_ovr_en() {
    vl::wr(rxclk_rxhs_start_calibration_ovr_en, vl::rd(rxclk_rxhs_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_lbdata_clk_en_ovr() {
    vl::wr(rxclk_lbdata_clk_en_ovr, vl::rd(rxclk_lbdata_clk_en_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_on_ovr() {
    vl::wr(cb_on_ovr, vl::rd(cb_on_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_cb_dac_prog_ovr_en() {
    vl::wr(cb_dac_prog_ovr_en, vl::rd(cb_dac_prog_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_lb_res_dp_en_if() {
    vl::wr(rx0_lb_res_dp_en_if, vl::rd(rx0_lb_res_dp_en_if_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rxclk_rxhs_osc_res_en_ovr_en() {
    vl::wr(rxclk_rxhs_osc_res_en_ovr_en, vl::rd(rxclk_rxhs_osc_res_en_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx0_rxhs_ddl_tune_ovr() {
    vl::wr(rx0_rxhs_ddl_tune_ovr, vl::rd(rx0_rxhs_ddl_tune_ovr_rw) & vl::rep<5>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx3_start_calibration_ovr() {
    vl::wr(rx3_start_calibration_ovr, vl::rd(rx3_start_calibration_ovr_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx2_start_calibration_ovr_en() {
    vl::wr(rx2_start_calibration_ovr_en, vl::rd(rx2_start_calibration_ovr_en_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx1_rxhs_ddl_tune_en_bypass() {
    vl::wr(rx1_rxhs_ddl_tune_en_bypass, vl::rd(rx1_rxhs_ddl_tune_en_bypass_rw) & vl::rep<1>(vl::rd(shutdownz_tester)));
}

void mipi_4_rx_dphy_ns_dphy4rxtester::assign_rx_rxhs_setr_ovr() {
    vl::wr(rx_rxhs_setr_ovr, vl::rd(rx_rxhs_setr_ovr_rw) & vl::rep<3>(vl::rd(shutdownz_tester)));
}

// %%CREG RDWR_RX_SYS_0 - System configuration
// OFFSET: 0x1
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] hsfreqrange_ovr_en_rw - HS frequency range of operation selection override
// RESET: 0x00
// [4:1] target_state_rw__3__0__ -
// NOTES: Target FSM state to which FSM will move when force_state = 1'b1
// <br>4'b0000: POWERDWN: Power Down
// <br>4'b0001: BGPON: Common block enable
// <br>4'b0010: CAL_TYPE: Termination Calibration Type
// <br>4'b0011: BURNIN_CAL: Termination Calibration without rext
// <br>4'b0100: TERMCAL: Termination Calibration with rext
// <br>4'b0101: OFFSETCAL  Clock Lane Offset Calibration
// <br>4'b0110: OFFSET_LANE  Data Lane Offset Calibration
// <br>4'b0111: IDLE: system boot completed
// <br>4'b1000: ULP: system in global ULP
// <br>4'b1001: DDLTUNNING: DDL tunning Calibration
// <br>4'b1010: SKEW_BACKWARD: Backwards Compatibility Skew Compensation
// <br>all others: reserved
// RESET: 0x00
// [0] force_state_rw - Force FSM to move to target state
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(hsfreqrange_ovr_en_rw), vl::rd(target_state_rw), vl::rd(force_state_rw));
    testdouts_0[0].write(v_.bit(0).sl());
    testdouts_1[0].write(v_.bit(1).sl());
    testdouts_2[0].write(v_.bit(2).sl());
    testdouts_3[0].write(v_.bit(3).sl());
    testdouts_4[0].write(v_.bit(4).sl());
    testdouts_5[0].write(v_.bit(5).sl());
    testdouts_6[0].write(v_.bit(6).sl());
    testdouts_7[0].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_1 - System configuration
// OFFSET: 0x2
// ATTR: RW, CUST
// FIELDS:
// [7] timebase_ovr_en_rw - 4us timebase override enable.
// RESET: 0x00
// [6:0] hsfreqrange_ovr_rw__6__0__ - HS frequency range of operation selection override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_2() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x2ULL))) & vl::cat(vl::rd(timebase_ovr_en_rw), vl::rd(hsfreqrange_ovr_rw));
    testdouts_0[1].write(v_.bit(0).sl());
    testdouts_1[1].write(v_.bit(1).sl());
    testdouts_2[1].write(v_.bit(2).sl());
    testdouts_3[1].write(v_.bit(3).sl());
    testdouts_4[1].write(v_.bit(4).sl());
    testdouts_5[1].write(v_.bit(5).sl());
    testdouts_6[1].write(v_.bit(6).sl());
    testdouts_7[1].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_2 - System configuration
// OFFSET: 0x3
// ATTR: RW, CUST
// FIELDS:
// [7:0] timebase_ovr_rw__7__0__ - 4us timebase override.
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_3() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x3ULL))) & vl::rd(timebase_ovr_rw);
    testdouts_0[2].write(v_.bit(0).sl());
    testdouts_1[2].write(v_.bit(1).sl());
    testdouts_2[2].write(v_.bit(2).sl());
    testdouts_3[2].write(v_.bit(3).sl());
    testdouts_4[2].write(v_.bit(4).sl());
    testdouts_5[2].write(v_.bit(5).sl());
    testdouts_6[2].write(v_.bit(6).sl());
    testdouts_7[2].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_3 - System configuration
// OFFSET: 0x4
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] cmp_polarity_rw - Comparator polarity
// RESET: 0x00
// [4] noext_burnin_res_cal_rw - Selection between type of calibration (if 1, no REXT will be done)
// RESET: 0x00
// [3] hsdirect_rw - Allows to set PHY in HS mode without going through LP states
// RESET: 0x00
// [2:0] reserved_1 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_4() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x4ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(cmp_polarity_rw), vl::rd(noext_burnin_res_cal_rw), vl::rd(hsdirect_rw), vl::logic<3>(0x0ULL));
    testdouts_0[3].write(v_.bit(0).sl());
    testdouts_1[3].write(v_.bit(1).sl());
    testdouts_2[3].write(v_.bit(2).sl());
    testdouts_3[3].write(v_.bit(3).sl());
    testdouts_4[3].write(v_.bit(4).sl());
    testdouts_5[3].write(v_.bit(5).sl());
    testdouts_6[3].write(v_.bit(6).sl());
    testdouts_7[3].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_4 - System configuration
// OFFSET: 0x5
// ATTR: RW, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] hsrxtclkmisspd_ovr_en_rw - Tclk-miss detection override enable.
// RESET: 0x00
// [5] hsrxtclkmisspd_ovr_rw - Tclk-miss detection override.
// RESET: 0x00
// [4:3] tclk_miss_opt_rw__1__0__ - Allows optimization of Tclk-miss detection structure response. Should only be used for lower cfg_clk frequencies and according to databook table.
// RESET: 0x03
// [2:0] number_of_ready_count_rw__2__0__ - Latency of measurement of analog data for data lane offset cal after changing setting
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_5() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(hsrxtclkmisspd_ovr_en_rw), vl::rd(hsrxtclkmisspd_ovr_rw), vl::rd(tclk_miss_opt_rw), vl::rd(number_of_ready_count_rw));
    testdouts_0[4].write(v_.bit(0).sl());
    testdouts_1[4].write(v_.bit(1).sl());
    testdouts_2[4].write(v_.bit(2).sl());
    testdouts_3[4].write(v_.bit(3).sl());
    testdouts_4[4].write(v_.bit(4).sl());
    testdouts_5[4].write(v_.bit(5).sl());
    testdouts_6[4].write(v_.bit(6).sl());
    testdouts_7[4].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_5 - System configuration
// OFFSET: 0x6
// ATTR: RW, CUST
// FIELDS:
// [7:0] offset_comp_threshold_rw__7__0__ - 1s  0s threshold to increase/decrease offset voltage setting
// RESET: 0x3F
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_6() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x6ULL))) & vl::rd(offset_comp_threshold_rw);
    testdouts_0[5].write(v_.bit(0).sl());
    testdouts_1[5].write(v_.bit(1).sl());
    testdouts_2[5].write(v_.bit(2).sl());
    testdouts_3[5].write(v_.bit(3).sl());
    testdouts_4[5].write(v_.bit(4).sl());
    testdouts_5[5].write(v_.bit(5).sl());
    testdouts_6[5].write(v_.bit(6).sl());
    testdouts_7[5].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_6 - System configuration
// OFFSET: 0x7
// ATTR: RW, CUST
// FIELDS:
// [7:0] tolerance_rw__7__0__ - Tolerance of offset comparator threshold (register 0x76)
// RESET: 0x02
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_7() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7ULL))) & vl::rd(tolerance_rw);
    testdouts_0[6].write(v_.bit(0).sl());
    testdouts_1[6].write(v_.bit(1).sl());
    testdouts_2[6].write(v_.bit(2).sl());
    testdouts_3[6].write(v_.bit(3).sl());
    testdouts_4[6].write(v_.bit(4).sl());
    testdouts_5[6].write(v_.bit(5).sl());
    testdouts_6[6].write(v_.bit(6).sl());
    testdouts_7[6].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_7 - System configuration
// OFFSET: 0x8
// ATTR: RW, CUST
// FIELDS:
// [7] tclk_miss_div2_ovr_en_rw - Use divider by 2 on Tclk-miss detection circuit override.
// RESET: 0x00
// [6] tclk_miss_div2_ovr_rw - Use divider by 2 on Tclk-miss detection circuit override.
// RESET: 0x00
// [5] deskew_polarity_rw - De-skew Calibration pattern control (only used in backwards compatibility)
// RESET: 0x00
// [4:3] deskew_latency_rw__1__0__ -
// NOTES: number of cycles
// <br>2'b00: 3 cycles
// <br>2'b01: 4 cycles
// <br>2'b10: 5 cycles
// <br>2'b11: 6 cycles
// RESET: 0x03
// [2] reserved_0 - Reserved field
// RESET: 0x00
// [1] skew_mux_sel_rw - Selects between auto mode and manual selection of de-skew cal mechanism
// RESET: 0x00
// [0] skew_mux_run_rw - Selects type of de-skew calibration mode to run (internal or burst based). For 1.5gbps | 2.0gbps. 0 is for backwards compatibility (requires skew_mux_sel = 1'b1)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_8() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x8ULL))) & vl::cat(vl::rd(tclk_miss_div2_ovr_en_rw), vl::rd(tclk_miss_div2_ovr_rw), vl::rd(deskew_polarity_rw), vl::rd(deskew_latency_rw), vl::logic<1>(0x0ULL), vl::rd(skew_mux_sel_rw), vl::rd(skew_mux_run_rw));
    testdouts_0[7].write(v_.bit(0).sl());
    testdouts_1[7].write(v_.bit(1).sl());
    testdouts_2[7].write(v_.bit(2).sl());
    testdouts_3[7].write(v_.bit(3).sl());
    testdouts_4[7].write(v_.bit(4).sl());
    testdouts_5[7].write(v_.bit(5).sl());
    testdouts_6[7].write(v_.bit(6).sl());
    testdouts_7[7].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_8 - System configuration
// OFFSET: 0x9
// ATTR: RW, CUST
// FIELDS:
// [7] tafailsafez_rw - Enable error contention detection in all LP modes
// RESET: 0x00
// [6:5] aligner_dk_cnf_rw__1__0__ - aligner config
// RESET: 0x00
// [4] noalign_error_rw - No alignment error bit. Selects how errsotsynchs_* behaves (if 1 only flag own lane's alignment, if 0, all errsotsynchs_* behave as a single link)
// RESET: 0x00
// [3:2] deskew_jump2steps_rw__1__0__ - De-skew Calibration steps control. Jumps 2 steps at a time in the de-skew calibration machine (reduces calibration time but results in higher sampling error)
// RESET: 0x00
// [1] deskew_overflow_rw - Feature that forces de-skew algorithm convergence by setting 2nd edge pointer to step 31 (for situations where eventual DDL variation over PVT could cause potential failures).
// RESET: 0x00
// [0] deskew_numedges_update_rw -
// NOTES: Procedure
// <br>Set deskew_nemedge_update to 1'b0;
// <br>Change deskew_numedges to desired value;
// <br>Set deskew_nemedge_update to 1'b1;
// <br>Circuitry ensures synchronous update of internal counters
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_9() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9ULL))) & vl::cat(vl::rd(tafailsafez_rw), vl::rd(aligner_dk_cnf_rw), vl::rd(noalign_error_rw), vl::rd(deskew_jump2steps_rw), vl::rd(deskew_overflow_rw), vl::rd(deskew_numedges_update_rw));
    testdouts_0[8].write(v_.bit(0).sl());
    testdouts_1[8].write(v_.bit(1).sl());
    testdouts_2[8].write(v_.bit(2).sl());
    testdouts_3[8].write(v_.bit(3).sl());
    testdouts_4[8].write(v_.bit(4).sl());
    testdouts_5[8].write(v_.bit(5).sl());
    testdouts_6[8].write(v_.bit(6).sl());
    testdouts_7[8].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_9 - System configuration
// OFFSET: 0xA
// ATTR: RW, CUST
// FIELDS:
// [7:0] deskew_numedges_rw__7__0__ - De-skew accumulator size (sinousoidal jitter tolerance)
// RESET: 0x40
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_10() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xaULL))) & vl::rd(deskew_numedges_rw);
    testdouts_0[9].write(v_.bit(0).sl());
    testdouts_1[9].write(v_.bit(1).sl());
    testdouts_2[9].write(v_.bit(2).sl());
    testdouts_3[9].write(v_.bit(3).sl());
    testdouts_4[9].write(v_.bit(4).sl());
    testdouts_5[9].write(v_.bit(5).sl());
    testdouts_6[9].write(v_.bit(6).sl());
    testdouts_7[9].write(v_.bit(7).sl());
}

// %%CREG RD_RX_SYS_0 - System configuration observability
// OFFSET: 0x1E
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] hsdirect_if - Direct HS indication
// RESET: 0x00
// [4] cb_det_supply - Supply presence detection
// RESET: 0x00
// [3:0] state_main__3__0__ - Main FSM state
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_11() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1eULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(hsdirect_if), vl::rd(cb_det_supply), vl::rd(state_main));
    testdouts_0[10].write(v_.bit(0).sl());
    testdouts_1[10].write(v_.bit(1).sl());
    testdouts_2[10].write(v_.bit(2).sl());
    testdouts_3[10].write(v_.bit(3).sl());
    testdouts_4[10].write(v_.bit(4).sl());
    testdouts_5[10].write(v_.bit(5).sl());
    testdouts_6[10].write(v_.bit(6).sl());
    testdouts_7[10].write(v_.bit(7).sl());
}

// %%CREG RD_RX_SYS_1 - System configuration observability
// OFFSET: 0x1F
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:0] hsfreqrange__6__0__ - HS frequency range selected
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_12() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1fULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(hsfreqrange));
    testdouts_0[11].write(v_.bit(0).sl());
    testdouts_1[11].write(v_.bit(1).sl());
    testdouts_2[11].write(v_.bit(2).sl());
    testdouts_3[11].write(v_.bit(3).sl());
    testdouts_4[11].write(v_.bit(4).sl());
    testdouts_5[11].write(v_.bit(5).sl());
    testdouts_6[11].write(v_.bit(6).sl());
    testdouts_7[11].write(v_.bit(7).sl());
}

// %%CREG RD_RX_SYS_2 - System configuration observability
// OFFSET: 0x20
// ATTR: READ, CUST
// FIELDS:
// [7:0] timebase__7__0__ - 4us timebase
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_13() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x20ULL))) & vl::rd(timebase);
    testdouts_0[12].write(v_.bit(0).sl());
    testdouts_1[12].write(v_.bit(1).sl());
    testdouts_2[12].write(v_.bit(2).sl());
    testdouts_3[12].write(v_.bit(3).sl());
    testdouts_4[12].write(v_.bit(4).sl());
    testdouts_5[12].write(v_.bit(5).sl());
    testdouts_6[12].write(v_.bit(6).sl());
    testdouts_7[12].write(v_.bit(7).sl());
}

// %%CREG RD_RX_SYS_3 - System configuration observability
// OFFSET: 0x21
// ATTR: READ, CUST
// FIELDS:
// [7] ulpsactivenot_global - Global ULPS entry signal (from all lanes)
// RESET: 0x00
// [6] txulpsexit_global - Global ULPS exit signal (from all lanes)
// RESET: 0x00
// [5] cb_cal_en_ifpowerctrl - Output of main FSM signaling termination calibration enable
// RESET: 0x00
// [4] lptxenpulldwn_ifpowerctrl - Main FSM's LP-TX pull down enable
// RESET: 0x00
// [3] lptxdin_ifpowerctrl - Main FSM's LP-TX data in
// RESET: 0x00
// [2] lptxena_ifpowerctrl - Main FSM's LP-TX driver enable
// RESET: 0x00
// [1] lptxpon_ifpowerctrl - Main FSM's LP-TX driver power on
// RESET: 0x00
// [0] bintpon_ifpowerctrl - Main FSM's level shifter power on
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_14() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x21ULL))) & vl::cat(vl::rd(ulpsactivenot_global), vl::rd(txulpsexit_global), vl::rd(cb_cal_en_ifpowerctrl), vl::rd(lptxenpulldwn_ifpowerctrl), vl::rd(lptxdin_ifpowerctrl), vl::rd(lptxena_ifpowerctrl), vl::rd(lptxpon_ifpowerctrl), vl::rd(bintpon_ifpowerctrl));
    testdouts_0[13].write(v_.bit(0).sl());
    testdouts_1[13].write(v_.bit(1).sl());
    testdouts_2[13].write(v_.bit(2).sl());
    testdouts_3[13].write(v_.bit(3).sl());
    testdouts_4[13].write(v_.bit(4).sl());
    testdouts_5[13].write(v_.bit(5).sl());
    testdouts_6[13].write(v_.bit(6).sl());
    testdouts_7[13].write(v_.bit(7).sl());
}

// %%CREG RD_CONT_DATA_0 - IO continuity feature internal observability
// OFFSET: 0x23
// ATTR: READ, CUST
// FIELDS:
// [7:0] cont_data__7__0__ - top level cont_data[7:0] observability
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_15() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x23ULL))) & vl::rd(cont_data).slice<8>(0);
    testdouts_0[14].write(v_.bit(0).sl());
    testdouts_1[14].write(v_.bit(1).sl());
    testdouts_2[14].write(v_.bit(2).sl());
    testdouts_3[14].write(v_.bit(3).sl());
    testdouts_4[14].write(v_.bit(4).sl());
    testdouts_5[14].write(v_.bit(5).sl());
    testdouts_6[14].write(v_.bit(6).sl());
    testdouts_7[14].write(v_.bit(7).sl());
}

// %%CREG RD_CONT_DATA_1 - IO continuity feature internal observability
// OFFSET: 0x24
// ATTR: READ, CUST
// FIELDS:
// [7:3] reserved_0 - Reserved field
// RESET: 0x00
// [2:0] cont_data__10__8__ - top level cont_data[10:8] observability
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_16() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x24ULL))) & vl::cat(vl::logic<5>(0x0ULL), vl::rd(cont_data).slice<3>(8));
    testdouts_0[15].write(v_.bit(0).sl());
    testdouts_1[15].write(v_.bit(1).sl());
    testdouts_2[15].write(v_.bit(2).sl());
    testdouts_3[15].write(v_.bit(3).sl());
    testdouts_4[15].write(v_.bit(4).sl());
    testdouts_5[15].write(v_.bit(5).sl());
    testdouts_6[15].write(v_.bit(6).sl());
    testdouts_7[15].write(v_.bit(7).sl());
}

// %%CREG RD_RX_SYS_4 - RX System signal observation
// OFFSET: 0x25
// ATTR: READ, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] rx_rxhs_prog__1__0__ - Analog programmability
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_17() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x25ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(rx_rxhs_prog));
    testdouts_0[16].write(v_.bit(0).sl());
    testdouts_1[16].write(v_.bit(1).sl());
    testdouts_2[16].write(v_.bit(2).sl());
    testdouts_3[16].write(v_.bit(3).sl());
    testdouts_4[16].write(v_.bit(4).sl());
    testdouts_5[16].write(v_.bit(5).sl());
    testdouts_6[16].write(v_.bit(6).sl());
    testdouts_7[16].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_DDL_0 - DDL programmability
// OFFSET: 0x37
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx_ddl_offset_error_rw__7__0__ - bits [7:0]: RX DDL offset error
// RESET: 0x32
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_18() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x37ULL))) & vl::rd(rx_ddl_offset_error_rw).slice<8>(0);
    testdouts_0[17].write(v_.bit(0).sl());
    testdouts_1[17].write(v_.bit(1).sl());
    testdouts_2[17].write(v_.bit(2).sl());
    testdouts_3[17].write(v_.bit(3).sl());
    testdouts_4[17].write(v_.bit(4).sl());
    testdouts_5[17].write(v_.bit(5).sl());
    testdouts_6[17].write(v_.bit(6).sl());
    testdouts_7[17].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_DDL_1 - DDL programmability
// OFFSET: 0x38
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx_ddl_offset_error_rw__11__8__ - bits [7:0]: RX DDL offset error
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_19() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x38ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx_ddl_offset_error_rw).slice<4>(8));
    testdouts_0[18].write(v_.bit(0).sl());
    testdouts_1[18].write(v_.bit(1).sl());
    testdouts_2[18].write(v_.bit(2).sl());
    testdouts_3[18].write(v_.bit(3).sl());
    testdouts_4[18].write(v_.bit(4).sl());
    testdouts_5[18].write(v_.bit(5).sl());
    testdouts_6[18].write(v_.bit(6).sl());
    testdouts_7[18].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYS_DDL_2 - DDL programmability
// OFFSET: 0x39
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] rx_rxhs_ddl_clk_out_div_rw__1__0__ - RX HS DDL output clock divider bits
// RESET: 0x02
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_20() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x39ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(rx_rxhs_ddl_clk_out_div_rw));
    testdouts_0[19].write(v_.bit(0).sl());
    testdouts_1[19].write(v_.bit(1).sl());
    testdouts_2[19].write(v_.bit(2).sl());
    testdouts_3[19].write(v_.bit(3).sl());
    testdouts_4[19].write(v_.bit(4).sl());
    testdouts_5[19].write(v_.bit(5).sl());
    testdouts_6[19].write(v_.bit(6).sl());
    testdouts_7[19].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_0 - System timers
// OFFSET: 0x4E
// ATTR: RW, CUST
// FIELDS:
// [7:0] timeinitbg_rw__7__0__ - Timer is loaded with value 10'bXXXXXXXX00
// RESET: 0x10
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_21() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x4eULL))) & vl::rd(timeinitbg_rw).slice<8>(0);
    testdouts_0[20].write(v_.bit(0).sl());
    testdouts_1[20].write(v_.bit(1).sl());
    testdouts_2[20].write(v_.bit(2).sl());
    testdouts_3[20].write(v_.bit(3).sl());
    testdouts_4[20].write(v_.bit(4).sl());
    testdouts_5[20].write(v_.bit(5).sl());
    testdouts_6[20].write(v_.bit(6).sl());
    testdouts_7[20].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_1 - System timers
// OFFSET: 0x4F
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] timeinitbg_rw__9__8__ - Timer is loaded with value 10'bXXXXXXXX00
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_22() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x4fULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(timeinitbg_rw).slice<2>(8));
    testdouts_0[21].write(v_.bit(0).sl());
    testdouts_1[21].write(v_.bit(1).sl());
    testdouts_2[21].write(v_.bit(2).sl());
    testdouts_3[21].write(v_.bit(3).sl());
    testdouts_4[21].write(v_.bit(4).sl());
    testdouts_5[21].write(v_.bit(5).sl());
    testdouts_6[21].write(v_.bit(6).sl());
    testdouts_7[21].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_2 - System timers
// OFFSET: 0x50
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5:0] ttago_rw__5__0__ - Number of cycles that the TX drives the bridge state during a turnaround procedure
// RESET: 0x03
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_23() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x50ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(ttago_rw));
    testdouts_0[22].write(v_.bit(0).sl());
    testdouts_1[22].write(v_.bit(1).sl());
    testdouts_2[22].write(v_.bit(2).sl());
    testdouts_3[22].write(v_.bit(3).sl());
    testdouts_4[22].write(v_.bit(4).sl());
    testdouts_5[22].write(v_.bit(5).sl());
    testdouts_6[22].write(v_.bit(6).sl());
    testdouts_7[22].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_3 - System timers
// OFFSET: 0x51
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5:0] ttasure_rw__5__0__ - Number of cycles that the RX waits after a bridge state has been detected during a turnaround procedure
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_24() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x51ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(ttasure_rw));
    testdouts_0[23].write(v_.bit(0).sl());
    testdouts_1[23].write(v_.bit(1).sl());
    testdouts_2[23].write(v_.bit(2).sl());
    testdouts_3[23].write(v_.bit(3).sl());
    testdouts_4[23].write(v_.bit(4).sl());
    testdouts_5[23].write(v_.bit(5).sl());
    testdouts_6[23].write(v_.bit(6).sl());
    testdouts_7[23].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_4 - System timers
// OFFSET: 0x52
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5:0] ttawait_rw__5__0__ - Number of cycles that the PHY waits for until a turnaround request is processed
// RESET: 0x32
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_25() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x52ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(ttawait_rw));
    testdouts_0[24].write(v_.bit(0).sl());
    testdouts_1[24].write(v_.bit(1).sl());
    testdouts_2[24].write(v_.bit(2).sl());
    testdouts_3[24].write(v_.bit(3).sl());
    testdouts_4[24].write(v_.bit(4).sl());
    testdouts_5[24].write(v_.bit(5).sl());
    testdouts_6[24].write(v_.bit(6).sl());
    testdouts_7[24].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_5 - System timers
// OFFSET: 0x53
// ATTR: RW, CUST
// FIELDS:
// [7:0] twakeupulpm_rw__7__0__ - Timer is loaded with value 10'bXXXXXXXX11
// RESET: 0x44
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_26() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x53ULL))) & vl::rd(twakeupulpm_rw).slice<8>(0);
    testdouts_0[25].write(v_.bit(0).sl());
    testdouts_1[25].write(v_.bit(1).sl());
    testdouts_2[25].write(v_.bit(2).sl());
    testdouts_3[25].write(v_.bit(3).sl());
    testdouts_4[25].write(v_.bit(4).sl());
    testdouts_5[25].write(v_.bit(5).sl());
    testdouts_6[25].write(v_.bit(6).sl());
    testdouts_7[25].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_6 - System timers
// OFFSET: 0x54
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] twakeupulpm_rw__9__8__ - Timer is loaded with value 10'bXXXXXXXX11
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_27() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x54ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(twakeupulpm_rw).slice<2>(8));
    testdouts_0[26].write(v_.bit(0).sl());
    testdouts_1[26].write(v_.bit(1).sl());
    testdouts_2[26].write(v_.bit(2).sl());
    testdouts_3[26].write(v_.bit(3).sl());
    testdouts_4[26].write(v_.bit(4).sl());
    testdouts_5[26].write(v_.bit(5).sl());
    testdouts_6[26].write(v_.bit(6).sl());
    testdouts_7[26].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_7 - System timers
// OFFSET: 0x55
// ATTR: RW, CUST
// FIELDS:
// [7:0] txdindlytime_rw__7__0__ - Timer is loaded with value 10'bXXXXXXXX00
// RESET: 0x1C
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_28() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x55ULL))) & vl::rd(txdindlytime_rw).slice<8>(0);
    testdouts_0[27].write(v_.bit(0).sl());
    testdouts_1[27].write(v_.bit(1).sl());
    testdouts_2[27].write(v_.bit(2).sl());
    testdouts_3[27].write(v_.bit(3).sl());
    testdouts_4[27].write(v_.bit(4).sl());
    testdouts_5[27].write(v_.bit(5).sl());
    testdouts_6[27].write(v_.bit(6).sl());
    testdouts_7[27].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_8 - System timers
// OFFSET: 0x56
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] txdindlytime_rw__9__8__ - Timer is loaded with value 10'bXXXXXXXX00
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_29() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x56ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(txdindlytime_rw).slice<2>(8));
    testdouts_0[28].write(v_.bit(0).sl());
    testdouts_1[28].write(v_.bit(1).sl());
    testdouts_2[28].write(v_.bit(2).sl());
    testdouts_3[28].write(v_.bit(3).sl());
    testdouts_4[28].write(v_.bit(4).sl());
    testdouts_5[28].write(v_.bit(5).sl());
    testdouts_6[28].write(v_.bit(6).sl());
    testdouts_7[28].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_9 - System timers
// OFFSET: 0x57
// ATTR: RW, CUST
// FIELDS:
// [7:0] txstarttime_rw__7__0__ - Timer is loaded with value 10'bXXXXXXXX00
// RESET: 0x10
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_30() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x57ULL))) & vl::rd(txstarttime_rw).slice<8>(0);
    testdouts_0[29].write(v_.bit(0).sl());
    testdouts_1[29].write(v_.bit(1).sl());
    testdouts_2[29].write(v_.bit(2).sl());
    testdouts_3[29].write(v_.bit(3).sl());
    testdouts_4[29].write(v_.bit(4).sl());
    testdouts_5[29].write(v_.bit(5).sl());
    testdouts_6[29].write(v_.bit(6).sl());
    testdouts_7[29].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_10 - System timers
// OFFSET: 0x58
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] txstarttime_rw__9__8__ - Timer is loaded with value 10'bXXXXXXXX00
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_31() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x58ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(txstarttime_rw).slice<2>(8));
    testdouts_0[30].write(v_.bit(0).sl());
    testdouts_1[30].write(v_.bit(1).sl());
    testdouts_2[30].write(v_.bit(2).sl());
    testdouts_3[30].write(v_.bit(3).sl());
    testdouts_4[30].write(v_.bit(4).sl());
    testdouts_5[30].write(v_.bit(5).sl());
    testdouts_6[30].write(v_.bit(6).sl());
    testdouts_7[30].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_SYSTIMERS_11 - System timers
// OFFSET: 0x59
// ATTR: RW, CUST
// FIELDS:
// [7:0] rxpontime_rw__7__0__ - Timer used in offset calibration for state machine power up.
// RESET: 0x05
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_32() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x59ULL))) & vl::rd(rxpontime_rw);
    testdouts_0[31].write(v_.bit(0).sl());
    testdouts_1[31].write(v_.bit(1).sl());
    testdouts_2[31].write(v_.bit(2).sl());
    testdouts_3[31].write(v_.bit(3).sl());
    testdouts_4[31].write(v_.bit(4).sl());
    testdouts_5[31].write(v_.bit(5).sl());
    testdouts_6[31].write(v_.bit(6).sl());
    testdouts_7[31].write(v_.bit(7).sl());
}

// %%CREG RD_RX_SYSTIMERS_0 - System timers
// OFFSET: 0x7A
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:0] hsrxthssettle_datalanes__6__0__ - HS RX THSSettle Timing Counter
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_33() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7aULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(hsrxthssettle_datalanes));
    testdouts_0[32].write(v_.bit(0).sl());
    testdouts_1[32].write(v_.bit(1).sl());
    testdouts_2[32].write(v_.bit(2).sl());
    testdouts_3[32].write(v_.bit(3).sl());
    testdouts_4[32].write(v_.bit(4).sl());
    testdouts_5[32].write(v_.bit(5).sl());
    testdouts_6[32].write(v_.bit(6).sl());
    testdouts_7[32].write(v_.bit(7).sl());
}

// %%CREG RD_RX_RX_STARTUP_OBS_0 - System startp observability
// OFFSET: 0xC7
// ATTR: READ, CUST
// FIELDS:
// [7] hsrxpon_clklane_ifpowerctrl - Output of main FSM enabling clock lane HS receiver analog circuitry
// RESET: 0x00
// [6] hsrxencal_lane_offset_ifpowerctrl - Output of main FSM enabling digital data lane offset calibration machine
// RESET: 0x00
// [5] clk_en_lanes - Clock enable lanes
// RESET: 0x00
// [4] des_en_for_datalane_offset_cal_ifpowerctrl - Output of main FSM enabling feed of clock lane receivers with internal oscillator signal
// RESET: 0x00
// [3] rxlane_feed_int_clk_ifpowerctrl - Output of main FSM enabling feed of data lane receivers with internal oscillator signal
// RESET: 0x00
// [2] rxclk_rxhs_feed_int_clk_if - Observes the selection between internal clock source or external (receiver) override mux output
// RESET: 0x00
// [1] rxclk_feed_int_clk_ifpowerctrl - Output of main FSM enabling feed of clock lane receivers with internal oscillator signal
// RESET: 0x00
// [0] burnin_cal_en_if - Burnin oscillator enable signal
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_34() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xc7ULL))) & vl::cat(vl::rd(hsrxpon_clklane_ifpowerctrl), vl::rd(hsrxencal_lane_offset_ifpowerctrl), vl::rd(clk_en_lanes), vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl), vl::rd(rxlane_feed_int_clk_ifpowerctrl), vl::rd(rxclk_rxhs_feed_int_clk_if), vl::rd(rxclk_feed_int_clk_ifpowerctrl), vl::rd(burnin_cal_en_if));
    testdouts_0[33].write(v_.bit(0).sl());
    testdouts_1[33].write(v_.bit(1).sl());
    testdouts_2[33].write(v_.bit(2).sl());
    testdouts_3[33].write(v_.bit(3).sl());
    testdouts_4[33].write(v_.bit(4).sl());
    testdouts_5[33].write(v_.bit(5).sl());
    testdouts_6[33].write(v_.bit(6).sl());
    testdouts_7[33].write(v_.bit(7).sl());
}

// %%CREG RD_RX_RX_STARTUP_OBS_1 - System startp observability
// OFFSET: 0xC8
// ATTR: READ, CUST
// FIELDS:
// [7] skew_back_comp_en - de-skew backwards compatibility observation
// RESET: 0x00
// [6] skew_back_comp_en_ifpowerctrl - Output of main FSM signalling backwards compatibility de-skew compensation enable
// RESET: 0x00
// [5] perform_skew - Internal signaling to distinguish de-skew calibration through internal oscillator or external burst training sequence (1 if compatibility mode)
// RESET: 0x00
// [4] rx_rxhs_compatibility_mode - compatibility mode observation
// RESET: 0x00
// [3] global_ddltunning_done - Flag indicating that the DDL calibrations for all the active lanes are finished
// RESET: 0x00
// [2] ddltunning_en_ifpowerctrl - Output of main FSM enabling DDL calibration
// RESET: 0x00
// [1] global_offset_cal_done - Flag indicating that the offset calibration for all the active lanes are finished
// RESET: 0x00
// [0] hsrxpon_datalane_ifpowerctrl - Output of main FSM enabling data lane HS receiver analog circuitry
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_35() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xc8ULL))) & vl::cat(vl::rd(skew_back_comp_en), vl::rd(skew_back_comp_en_ifpowerctrl), vl::rd(perform_skew), vl::rd(rx_rxhs_compatibility_mode), vl::rd(global_ddltunning_done), vl::rd(ddltunning_en_ifpowerctrl), vl::rd(global_offset_cal_done), vl::rd(hsrxpon_datalane_ifpowerctrl));
    testdouts_0[34].write(v_.bit(0).sl());
    testdouts_1[34].write(v_.bit(1).sl());
    testdouts_2[34].write(v_.bit(2).sl());
    testdouts_3[34].write(v_.bit(3).sl());
    testdouts_4[34].write(v_.bit(4).sl());
    testdouts_5[34].write(v_.bit(5).sl());
    testdouts_6[34].write(v_.bit(6).sl());
    testdouts_7[34].write(v_.bit(7).sl());
}

// %%CREG RD_RX_RX_STARTUP_OBS_2 - System startp observability
// OFFSET: 0xC9
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] deskew_legacyz - De-skew algorithm currently enabled (1'b0 for backwards compatibility)
// RESET: 0x00
// [4] enable_lanes - Lanes enable observation
// RESET: 0x00
// [3:2] rx_rxhs_gmode__1__0__ - RX HS gmode observation
// RESET: 0x00
// [1] align_error - No alignment error bit. Selects how errsotsynchs_* behaves (if 1 only flag own lane's alignment, if 0, all errsotsynchs_* behave as a single link)
// RESET: 0x00
// [0] global_skew_cal_done - Flag indicating that the de-skew calibrations for all the active lanes are finished
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_36() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xc9ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(deskew_legacyz), vl::rd(enable_lanes), vl::rd(rx_rxhs_gmode), vl::rd(align_error), vl::rd(global_skew_cal_done));
    testdouts_0[35].write(v_.bit(0).sl());
    testdouts_1[35].write(v_.bit(1).sl());
    testdouts_2[35].write(v_.bit(2).sl());
    testdouts_3[35].write(v_.bit(3).sl());
    testdouts_4[35].write(v_.bit(4).sl());
    testdouts_5[35].write(v_.bit(5).sl());
    testdouts_6[35].write(v_.bit(6).sl());
    testdouts_7[35].write(v_.bit(7).sl());
}

// %%CREG RD_RX_RX_STARTUP_OBS_3 - System startp observability
// OFFSET: 0xCA
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] counter_for_des_en_if__3__0__ - Observation of the counter needed to meet GTP ( min and max clocklane settle time)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_37() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xcaULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(counter_for_des_en_if));
    testdouts_0[36].write(v_.bit(0).sl());
    testdouts_1[36].write(v_.bit(1).sl());
    testdouts_2[36].write(v_.bit(2).sl());
    testdouts_3[36].write(v_.bit(3).sl());
    testdouts_4[36].write(v_.bit(4).sl());
    testdouts_5[36].write(v_.bit(5).sl());
    testdouts_6[36].write(v_.bit(6).sl());
    testdouts_7[36].write(v_.bit(7).sl());
}

// %%CREG RD_RX_RX_STARTUP_OBS_4 - System startp observability
// OFFSET: 0xCB
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:0] hsfreqrange_top__6__0__ - hsfreqrange observation after mux override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_38() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xcbULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(hsfreqrange_top));
    testdouts_0[37].write(v_.bit(0).sl());
    testdouts_1[37].write(v_.bit(1).sl());
    testdouts_2[37].write(v_.bit(2).sl());
    testdouts_3[37].write(v_.bit(3).sl());
    testdouts_4[37].write(v_.bit(4).sl());
    testdouts_5[37].write(v_.bit(5).sl());
    testdouts_6[37].write(v_.bit(6).sl());
    testdouts_7[37].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_RX_STARTUP_OVR_0 - System startup observability
// OFFSET: 0xE0
// ATTR: RW, CUST
// FIELDS:
// [7] rx_rxhs_compatibility_mode_ovr_en_rw - compatibility mode override enable
// RESET: 0x00
// [6] bypass_skew_machine_rw - Bypass of de-skew machine
// RESET: 0x00
// [5] skew_back_comp_en_ovr_rw - de-skew backwards compatibility override enable value
// RESET: 0x00
// [4] skew_back_comp_en_ovr_en_rw - de-skew back comparator override enable control
// RESET: 0x00
// [3] bypass_ddltunning_machine_rw - Bypass DDL tunning machine
// RESET: 0x00
// [2] bypass_offset_machine_rw - Bypass offset machine bit
// RESET: 0x00
// [1] clk_en_lanes_tester_rw - Clock enable lanes for tester
// RESET: 0x00
// [0] clk_en_lanes_bypass_rw - Clock enable lanes bypass
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_39() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL))) & vl::cat(vl::rd(rx_rxhs_compatibility_mode_ovr_en_rw), vl::rd(bypass_skew_machine_rw), vl::rd(skew_back_comp_en_ovr_rw), vl::rd(skew_back_comp_en_ovr_en_rw), vl::rd(bypass_ddltunning_machine_rw), vl::rd(bypass_offset_machine_rw), vl::rd(clk_en_lanes_tester_rw), vl::rd(clk_en_lanes_bypass_rw));
    testdouts_0[38].write(v_.bit(0).sl());
    testdouts_1[38].write(v_.bit(1).sl());
    testdouts_2[38].write(v_.bit(2).sl());
    testdouts_3[38].write(v_.bit(3).sl());
    testdouts_4[38].write(v_.bit(4).sl());
    testdouts_5[38].write(v_.bit(5).sl());
    testdouts_6[38].write(v_.bit(6).sl());
    testdouts_7[38].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_RX_STARTUP_OVR_1 - System startup override
// OFFSET: 0xE1
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] deskew_legacyz_ovr_en_rw - De-skew algorithm override enable.
// RESET: 0x00
// [4] deskew_legacyz_ovr_rw - De-skew algorithm override (requires deskew_legacyz_ovr_en = 1'b1).
// RESET: 0x00
// [3] enable_lanes_ovr_rw - Lanes enable override (requires enable_lanes_ovr_e = 1'b1)
// RESET: 0x00
// [2] enable_lanes_ovr_en_rw - Lanes enable override enable
// RESET: 0x00
// [1] rxclk_rxhs_ddr_clk_en_bypass_rw - HS RX DDR clock enable bypass control
// RESET: 0x00
// [0] rx_rxhs_compatibility_mode_ovr_rw - compatibility mode override value (needs rx_rxhs_compatibility_mode_ovr_en = 1'b1)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_40() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xe1ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(deskew_legacyz_ovr_en_rw), vl::rd(deskew_legacyz_ovr_rw), vl::rd(enable_lanes_ovr_rw), vl::rd(enable_lanes_ovr_en_rw), vl::rd(rxclk_rxhs_ddr_clk_en_bypass_rw), vl::rd(rx_rxhs_compatibility_mode_ovr_rw));
    testdouts_0[39].write(v_.bit(0).sl());
    testdouts_1[39].write(v_.bit(1).sl());
    testdouts_2[39].write(v_.bit(2).sl());
    testdouts_3[39].write(v_.bit(3).sl());
    testdouts_4[39].write(v_.bit(4).sl());
    testdouts_5[39].write(v_.bit(5).sl());
    testdouts_6[39].write(v_.bit(6).sl());
    testdouts_7[39].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_RX_STARTUP_OVR_2 - System startup override
// OFFSET: 0xE2
// ATTR: RW, CUST
// FIELDS:
// [7:0] ddl_osc_freq_target_ovr_rw__7__0__ - DDL oscillation frequency override value ( Main )
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_41() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xe2ULL))) & vl::rd(ddl_osc_freq_target_ovr_rw).slice<8>(0);
    testdouts_0[40].write(v_.bit(0).sl());
    testdouts_1[40].write(v_.bit(1).sl());
    testdouts_2[40].write(v_.bit(2).sl());
    testdouts_3[40].write(v_.bit(3).sl());
    testdouts_4[40].write(v_.bit(4).sl());
    testdouts_5[40].write(v_.bit(5).sl());
    testdouts_6[40].write(v_.bit(6).sl());
    testdouts_7[40].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_RX_STARTUP_OVR_3 - System startup override
// OFFSET: 0xE3
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] ddl_osc_freq_target_ovr_rw__11__8__ - DDL oscillation frequency override value ( Main )
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_42() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xe3ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(ddl_osc_freq_target_ovr_rw).slice<4>(8));
    testdouts_0[41].write(v_.bit(0).sl());
    testdouts_1[41].write(v_.bit(1).sl());
    testdouts_2[41].write(v_.bit(2).sl());
    testdouts_3[41].write(v_.bit(3).sl());
    testdouts_4[41].write(v_.bit(4).sl());
    testdouts_5[41].write(v_.bit(5).sl());
    testdouts_6[41].write(v_.bit(6).sl());
    testdouts_7[41].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_RX_STARTUP_OVR_4 - System startup override
// OFFSET: 0xE4
// ATTR: RW, CUST
// FIELDS:
// [7:4] counter_for_des_en_config_if_rw__3__0__ - Override value of the counter used to meet the min and max clocklane settle time
// RESET: 0x00
// [3:2] rx_rxhs_gmode_if_ovr_rw__1__0__ - RX HS gmode override value
// RESET: 0x00
// [1] rx_rxhs_gmode_if_ovr_en_rw - RX HS gmode override enable control
// RESET: 0x00
// [0] ddl_osc_freq_target_ovr_en_rw - DDL oscillation frequency override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_43() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xe4ULL))) & vl::cat(vl::rd(counter_for_des_en_config_if_rw), vl::rd(rx_rxhs_gmode_if_ovr_rw), vl::rd(rx_rxhs_gmode_if_ovr_en_rw), vl::rd(ddl_osc_freq_target_ovr_en_rw));
    testdouts_0[42].write(v_.bit(0).sl());
    testdouts_1[42].write(v_.bit(1).sl());
    testdouts_2[42].write(v_.bit(2).sl());
    testdouts_3[42].write(v_.bit(3).sl());
    testdouts_4[42].write(v_.bit(4).sl());
    testdouts_5[42].write(v_.bit(5).sl());
    testdouts_6[42].write(v_.bit(6).sl());
    testdouts_7[42].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_RX_STARTUP_OVR_5 - System startup override
// OFFSET: 0xE5
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] counter_for_des_en_bypass_rw - Override enable of the counter used to meet the min and max clocklane settle time
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_44() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xe5ULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(counter_for_des_en_bypass_rw));
    testdouts_0[43].write(v_.bit(0).sl());
    testdouts_1[43].write(v_.bit(1).sl());
    testdouts_2[43].write(v_.bit(2).sl());
    testdouts_3[43].write(v_.bit(3).sl());
    testdouts_4[43].write(v_.bit(4).sl());
    testdouts_5[43].write(v_.bit(5).sl());
    testdouts_6[43].write(v_.bit(6).sl());
    testdouts_7[43].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_RX_STARTUP_OVR_17 - System startup override
// OFFSET: 0xF1
// ATTR: RW, CUST
// FIELDS:
// [7] hsrxthssettle_ovr_en_datalanes_rw - HS_SETTLE timer override enable  for datalanes
// RESET: 0x00
// [6:0] hsrxthssettle_datalanes_ovr_rw__6__0__ - HS_SETTLE timer override value   for datalanes
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_45() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xf1ULL))) & vl::cat(vl::rd(hsrxthssettle_ovr_en_datalanes_rw), vl::rd(hsrxthssettle_datalanes_ovr_rw));
    testdouts_0[44].write(v_.bit(0).sl());
    testdouts_1[44].write(v_.bit(1).sl());
    testdouts_2[44].write(v_.bit(2).sl());
    testdouts_3[44].write(v_.bit(3).sl());
    testdouts_4[44].write(v_.bit(4).sl());
    testdouts_5[44].write(v_.bit(5).sl());
    testdouts_6[44].write(v_.bit(6).sl());
    testdouts_7[44].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_0 - Data pattern generator configuration
// OFFSET: 0x107
// ATTR: RW, CUST
// FIELDS:
// [7:4] pg_mode_r_rw__3__0__ - Pattern Generator modes selection
// RESET: 0x07
// [3] pg_bist_error_rw - Automatic error insertion control for the pattern generator
// RESET: 0x00
// [2] pg_data_polarity_rw - Allows inversion of pattern generator's output data (for dp/dn swap)
// RESET: 0x00
// [1] pg_data_swap_rw - Allows reordering of pattern generator's output data ([7:0] -> [0:7])
// RESET: 0x00
// [0] pg_reg_err_r_rw - Trigger for Pattern generator error insertion
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_46() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x107ULL))) & vl::cat(vl::rd(pg_mode_r_rw), vl::rd(pg_bist_error_rw), vl::rd(pg_data_polarity_rw), vl::rd(pg_data_swap_rw), vl::rd(pg_reg_err_r_rw));
    testdouts_0[45].write(v_.bit(0).sl());
    testdouts_1[45].write(v_.bit(1).sl());
    testdouts_2[45].write(v_.bit(2).sl());
    testdouts_3[45].write(v_.bit(3).sl());
    testdouts_4[45].write(v_.bit(4).sl());
    testdouts_5[45].write(v_.bit(5).sl());
    testdouts_6[45].write(v_.bit(6).sl());
    testdouts_7[45].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_1 - Data pattern generator configuration
// OFFSET: 0x108
// ATTR: RW, CUST
// FIELDS:
// [7:0] pg_usert_pat_rw__7__0__ - Pattern Generator User Defined Data Control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_47() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x108ULL))) & vl::rd(pg_usert_pat_rw);
    testdouts_0[46].write(v_.bit(0).sl());
    testdouts_1[46].write(v_.bit(1).sl());
    testdouts_2[46].write(v_.bit(2).sl());
    testdouts_3[46].write(v_.bit(3).sl());
    testdouts_4[46].write(v_.bit(4).sl());
    testdouts_5[46].write(v_.bit(5).sl());
    testdouts_6[46].write(v_.bit(6).sl());
    testdouts_7[46].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_2 - Clock pattern generator configuration
// OFFSET: 0x109
// ATTR: RW, CUST
// FIELDS:
// [7:0] rxclk_lbck_data_if_rw__7__0__ - Clock pattern to be serialized (BIST requires a square wave clock pattern)
// RESET: 0xAA
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_48() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x109ULL))) & vl::rd(rxclk_lbck_data_if_rw);
    testdouts_0[47].write(v_.bit(0).sl());
    testdouts_1[47].write(v_.bit(1).sl());
    testdouts_2[47].write(v_.bit(2).sl());
    testdouts_3[47].write(v_.bit(3).sl());
    testdouts_4[47].write(v_.bit(4).sl());
    testdouts_5[47].write(v_.bit(5).sl());
    testdouts_6[47].write(v_.bit(6).sl());
    testdouts_7[47].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_3 - BIST configuration options
// OFFSET: 0x10A
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] bist_run_lane3_rw - Sets lane 3 to run in BIST
// RESET: 0x01
// [4] bist_run_lane2_rw - Sets lane 2 to run in BIST
// RESET: 0x01
// [3] bist_run_lane1_rw - Sets lane 1 to run in BIST
// RESET: 0x01
// [2] bist_run_lane0_rw - Sets lane 0 to run in BIST
// RESET: 0x01
// [1] bist_manual_rw - Internal way to enable BIST (in case top level pin is not available)
// RESET: 0x00
// [0] comp_after_strobes_rw - Leaves BIST machine running after completion
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_49() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x10aULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(bist_run_lane3_rw), vl::rd(bist_run_lane2_rw), vl::rd(bist_run_lane1_rw), vl::rd(bist_run_lane0_rw), vl::rd(bist_manual_rw), vl::rd(comp_after_strobes_rw));
    testdouts_0[48].write(v_.bit(0).sl());
    testdouts_1[48].write(v_.bit(1).sl());
    testdouts_2[48].write(v_.bit(2).sl());
    testdouts_3[48].write(v_.bit(3).sl());
    testdouts_4[48].write(v_.bit(4).sl());
    testdouts_5[48].write(v_.bit(5).sl());
    testdouts_6[48].write(v_.bit(6).sl());
    testdouts_7[48].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_4 - Data pattern generator configuration
// OFFSET: 0x10B
// ATTR: RW, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] rxclk_lbck_clk_kill_if_rw - Allows pulse skipping of clock serializer word clock (only for debug purposes).
// RESET: 0x00
// [5] rxclk_lbdata_clk_kill_if_rw - Allows pulse skipping of data serializer word clock (only for debug purposes).
// RESET: 0x00
// [4] force_state_bist_rw - Allows forcing the BISt machine state. target_state_bist must already be set before asserting force_state_bist.
// RESET: 0x00
// [3:0] target_state_bist_rw__3__0__ - BIST machine target state
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_50() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x10bULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rxclk_lbck_clk_kill_if_rw), vl::rd(rxclk_lbdata_clk_kill_if_rw), vl::rd(force_state_bist_rw), vl::rd(target_state_bist_rw));
    testdouts_0[49].write(v_.bit(0).sl());
    testdouts_1[49].write(v_.bit(1).sl());
    testdouts_2[49].write(v_.bit(2).sl());
    testdouts_3[49].write(v_.bit(3).sl());
    testdouts_4[49].write(v_.bit(4).sl());
    testdouts_5[49].write(v_.bit(5).sl());
    testdouts_6[49].write(v_.bit(6).sl());
    testdouts_7[49].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_OVR_0 - Clock serializer override control
// OFFSET: 0x110
// ATTR: RW, CUST
// FIELDS:
// [7] rxclk_lbck_serial_en_ovr_en_rw - Clock serializer enable override enable
// RESET: 0x00
// [6] rxclk_lbck_serial_en_ovr_rw - Clock serializer enable override
// RESET: 0x00
// [5] rxclk_lbck_serial_clk_en_ovr_en_rw - Clock serializer word clock ungating override enable
// RESET: 0x00
// [4] rxclk_lbck_serial_clk_en_ovr_rw - Clock serializer word clock ungating override
// RESET: 0x00
// [3] rxclk_lbck_div_en_ovr_en_rw - Clock serializer word clock divider enable override enable
// RESET: 0x00
// [2] rxclk_lbck_div_en_ovr_rw - Clock serializer word clock divider enable override
// RESET: 0x00
// [1] rxclk_lbck_clk_en_ovr_en_rw - Clock serializer word clock enable override enable
// RESET: 0x00
// [0] rxclk_lbck_clk_en_ovr_rw - Clock serializer word clock enable override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_51() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL))) & vl::cat(vl::rd(rxclk_lbck_serial_en_ovr_en_rw), vl::rd(rxclk_lbck_serial_en_ovr_rw), vl::rd(rxclk_lbck_serial_clk_en_ovr_en_rw), vl::rd(rxclk_lbck_serial_clk_en_ovr_rw), vl::rd(rxclk_lbck_div_en_ovr_en_rw), vl::rd(rxclk_lbck_div_en_ovr_rw), vl::rd(rxclk_lbck_clk_en_ovr_en_rw), vl::rd(rxclk_lbck_clk_en_ovr_rw));
    testdouts_0[50].write(v_.bit(0).sl());
    testdouts_1[50].write(v_.bit(1).sl());
    testdouts_2[50].write(v_.bit(2).sl());
    testdouts_3[50].write(v_.bit(3).sl());
    testdouts_4[50].write(v_.bit(4).sl());
    testdouts_5[50].write(v_.bit(5).sl());
    testdouts_6[50].write(v_.bit(6).sl());
    testdouts_7[50].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_OVR_1 - Data serializer override control
// OFFSET: 0x111
// ATTR: RW, CUST
// FIELDS:
// [7] rxclk_lbdata_serial_en_ovr_en_rw - Data serializer enable override enable
// RESET: 0x00
// [6] rxclk_lbdata_serial_en_ovr_rw - Data serializer enable override
// RESET: 0x00
// [5] rxclk_lbdata_serial_clk_en_ovr_en_rw - Data serializer word clock ungating override enable
// RESET: 0x00
// [4] rxclk_lbdata_serial_clk_en_ovr_rw - Data serializer word clock ungating override
// RESET: 0x00
// [3] rxclk_lbdata_div_en_ovr_en_rw - Data serializer word clock divider enable override enable
// RESET: 0x00
// [2] rxclk_lbdata_div_en_ovr_rw - Data serializer word clock divider enable override
// RESET: 0x00
// [1] rxclk_lbdata_clk_en_ovr_en_rw - Data serializer word clock enable override enable
// RESET: 0x00
// [0] rxclk_lbdata_clk_en_ovr_rw - Data serializer word clock enable override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_52() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL))) & vl::cat(vl::rd(rxclk_lbdata_serial_en_ovr_en_rw), vl::rd(rxclk_lbdata_serial_en_ovr_rw), vl::rd(rxclk_lbdata_serial_clk_en_ovr_en_rw), vl::rd(rxclk_lbdata_serial_clk_en_ovr_rw), vl::rd(rxclk_lbdata_div_en_ovr_en_rw), vl::rd(rxclk_lbdata_div_en_ovr_rw), vl::rd(rxclk_lbdata_clk_en_ovr_en_rw), vl::rd(rxclk_lbdata_clk_en_ovr_rw));
    testdouts_0[51].write(v_.bit(0).sl());
    testdouts_1[51].write(v_.bit(1).sl());
    testdouts_2[51].write(v_.bit(2).sl());
    testdouts_3[51].write(v_.bit(3).sl());
    testdouts_4[51].write(v_.bit(4).sl());
    testdouts_5[51].write(v_.bit(5).sl());
    testdouts_6[51].write(v_.bit(6).sl());
    testdouts_7[51].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_OVR_2 - Miscellaneous override control
// OFFSET: 0x112
// ATTR: RW, CUST
// FIELDS:
// [7:0] del_datalane_ovr_rw__7__0__ - Timer set in pulses of cfg_clk and should be set above 500ns.
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_53() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x112ULL))) & vl::rd(del_datalane_ovr_rw);
    testdouts_0[52].write(v_.bit(0).sl());
    testdouts_1[52].write(v_.bit(1).sl());
    testdouts_2[52].write(v_.bit(2).sl());
    testdouts_3[52].write(v_.bit(3).sl());
    testdouts_4[52].write(v_.bit(4).sl());
    testdouts_5[52].write(v_.bit(5).sl());
    testdouts_6[52].write(v_.bit(6).sl());
    testdouts_7[52].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_OVR_3 - Miscellaneous override control
// OFFSET: 0x113
// ATTR: RW, CUST
// FIELDS:
// [7] rx_rxhs_prog_ovr_en_rw - AFE internal clock routing attachment to receiver override enable.
// RESET: 0x00
// [6:5] rx_rxhs_prog_ovr_rw__1__0__ - bit [1] is unused and should be considered reserved.
// RESET: 0x00
// [4] rxclk_lb_sel_ovr_en_rw - Analog loopback multiplexer override enable.
// RESET: 0x00
// [3] rxclk_lb_sel_ovr_rw - Analog loopback multiplexer override. Used to force AFE to recover data from internal loopback driver.
// RESET: 0x00
// [2] fsm_trigger_ovr_en_rw - PLL boot completed flag override enable.
// RESET: 0x00
// [1] fsm_trigger_ovr_rw - PLL boot completed flag override. Used to bypass PLL boot sequence of BIST machine.
// RESET: 0x00
// [0] del_datalane_ovr_en_rw - Serializer timer override enable.
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_54() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x113ULL))) & vl::cat(vl::rd(rx_rxhs_prog_ovr_en_rw), vl::rd(rx_rxhs_prog_ovr_rw), vl::rd(rxclk_lb_sel_ovr_en_rw), vl::rd(rxclk_lb_sel_ovr_rw), vl::rd(fsm_trigger_ovr_en_rw), vl::rd(fsm_trigger_ovr_rw), vl::rd(del_datalane_ovr_en_rw));
    testdouts_0[53].write(v_.bit(0).sl());
    testdouts_1[53].write(v_.bit(1).sl());
    testdouts_2[53].write(v_.bit(2).sl());
    testdouts_3[53].write(v_.bit(3).sl());
    testdouts_4[53].write(v_.bit(4).sl());
    testdouts_5[53].write(v_.bit(5).sl());
    testdouts_6[53].write(v_.bit(6).sl());
    testdouts_7[53].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_BIST_PIPE_TIMER - Configuration timer for pipe clean while performing BIST
// OFFSET: 0x114
// ATTR: RW, CUST
// FIELDS:
// [7:0] pipe_clean_time_rw__7__0__ - Timer set in pulses of cfg_clk.
// RESET: 0x80
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_55() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x114ULL))) & vl::rd(pipe_clean_time_rw);
    testdouts_0[54].write(v_.bit(0).sl());
    testdouts_1[54].write(v_.bit(1).sl());
    testdouts_2[54].write(v_.bit(2).sl());
    testdouts_3[54].write(v_.bit(3).sl());
    testdouts_4[54].write(v_.bit(4).sl());
    testdouts_5[54].write(v_.bit(5).sl());
    testdouts_6[54].write(v_.bit(6).sl());
    testdouts_7[54].write(v_.bit(7).sl());
}

// %%CREG RD_RX_BIST_0 - BIST related observability
// OFFSET: 0x11E
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] rx3_error_latch_if - Lane 3 error indication
// RESET: 0x00
// [5] rx2_error_latch_if - Lane 2 error indication
// RESET: 0x00
// [4] rx1_error_latch_if - Lane 1 error indication
// RESET: 0x00
// [3] rx0_error_latch_if - Lane 0 error indication
// RESET: 0x00
// [2] pg_sel - Indication that Pattern Generator is enabled
// RESET: 0x00
// [1] pg_pat_strobe - Indication that a PRBS complete run has occurred
// RESET: 0x00
// [0] biston_lanes - Indication of bist enable to core
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_56() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x11eULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx3_error_latch_if), vl::rd(rx2_error_latch_if), vl::rd(rx1_error_latch_if), vl::rd(rx0_error_latch_if), vl::rd(pg_sel), vl::rd(pg_pat_strobe), vl::rd(biston_lanes));
    testdouts_0[55].write(v_.bit(0).sl());
    testdouts_1[55].write(v_.bit(1).sl());
    testdouts_2[55].write(v_.bit(2).sl());
    testdouts_3[55].write(v_.bit(3).sl());
    testdouts_4[55].write(v_.bit(4).sl());
    testdouts_5[55].write(v_.bit(5).sl());
    testdouts_6[55].write(v_.bit(6).sl());
    testdouts_7[55].write(v_.bit(7).sl());
}

// %%CREG RD_RX_BIST_1 - BIST related observability
// OFFSET: 0x11F
// ATTR: READ, CUST
// FIELDS:
// [7] rxclk_lbck_clk_en_if - Clock serializer divider reset
// RESET: 0x00
// [6] rxclk_lbck_div_en_if - Clock serializer divider enable
// RESET: 0x00
// [5] rxclk_lbck_serial_clk_en_if - Clock serializer clock enable
// RESET: 0x00
// [4] rxclk_lbck_serial_en_if - Clock serializer enable
// RESET: 0x00
// [3] rxclk_lbdata_clk_en_if - Data serializer divider reset
// RESET: 0x00
// [2] rxclk_lbdata_div_en_if - Data serializer divider enable
// RESET: 0x00
// [1] rxclk_lbdata_serial_clk_en_if - Data serializer clock enable
// RESET: 0x00
// [0] rxclk_lbdata_serial_en_if - Data serializer enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_57() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x11fULL))) & vl::cat(vl::rd(rxclk_lbck_clk_en_if), vl::rd(rxclk_lbck_div_en_if), vl::rd(rxclk_lbck_serial_clk_en_if), vl::rd(rxclk_lbck_serial_en_if), vl::rd(rxclk_lbdata_clk_en_if), vl::rd(rxclk_lbdata_div_en_if), vl::rd(rxclk_lbdata_serial_clk_en_if), vl::rd(rxclk_lbdata_serial_en_if));
    testdouts_0[56].write(v_.bit(0).sl());
    testdouts_1[56].write(v_.bit(1).sl());
    testdouts_2[56].write(v_.bit(2).sl());
    testdouts_3[56].write(v_.bit(3).sl());
    testdouts_4[56].write(v_.bit(4).sl());
    testdouts_5[56].write(v_.bit(5).sl());
    testdouts_6[56].write(v_.bit(6).sl());
    testdouts_7[56].write(v_.bit(7).sl());
}

// %%CREG RD_RX_BIST_2 - BIST related observability
// OFFSET: 0x120
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] error_gating_out - Signal that indicates that the detected errors in BIST are gated
// RESET: 0x00
// [5:2] bist_state__3__0__ - Data serializer divider enable
// RESET: 0x00
// [1] fsm_trigger - PHY startup done (BIST triggered)
// RESET: 0x00
// [0] rxclk_lb_sel_if - Loopback mux control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_58() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x120ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(error_gating_out), vl::rd(bist_state), vl::rd(fsm_trigger), vl::rd(rxclk_lb_sel_if));
    testdouts_0[57].write(v_.bit(0).sl());
    testdouts_1[57].write(v_.bit(1).sl());
    testdouts_2[57].write(v_.bit(2).sl());
    testdouts_3[57].write(v_.bit(3).sl());
    testdouts_4[57].write(v_.bit(4).sl());
    testdouts_5[57].write(v_.bit(5).sl());
    testdouts_6[57].write(v_.bit(6).sl());
    testdouts_7[57].write(v_.bit(7).sl());
}

// %%CREG RD_RX_BIST_3 - BIST related observability
// OFFSET: 0x121
// ATTR: READ, CUST
// FIELDS:
// [7:0] del_datalane__7__0__ - Delay between enabling clock and data serializer clocks
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_59() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x121ULL))) & vl::rd(del_datalane);
    testdouts_0[58].write(v_.bit(0).sl());
    testdouts_1[58].write(v_.bit(1).sl());
    testdouts_2[58].write(v_.bit(2).sl());
    testdouts_3[58].write(v_.bit(3).sl());
    testdouts_4[58].write(v_.bit(4).sl());
    testdouts_5[58].write(v_.bit(5).sl());
    testdouts_6[58].write(v_.bit(6).sl());
    testdouts_7[58].write(v_.bit(7).sl());
}

// %%CREG RD_RX_BIST_13 - BIST related observability
// OFFSET: 0x12C
// ATTR: READ, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] bistok - BIST OK flag (same as top level pin)
// RESET: 0x00
// [0] bistdone - BIST done flag (same as top level pin)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_60() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x12cULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(bistok), vl::rd(bistdone));
    testdouts_0[59].write(v_.bit(0).sl());
    testdouts_1[59].write(v_.bit(1).sl());
    testdouts_2[59].write(v_.bit(2).sl());
    testdouts_3[59].write(v_.bit(3).sl());
    testdouts_4[59].write(v_.bit(4).sl());
    testdouts_5[59].write(v_.bit(5).sl());
    testdouts_6[59].write(v_.bit(6).sl());
    testdouts_7[59].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_RX_DUAL_PHY_0 - TX dual PHY control
// OFFSET: 0x133
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] mastermacro_rw - PHY which has its own clock lane enabled should set this field to 1'b1 (master PHY). The other should set it to 1'b0 (slave PHY).
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_61() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x133ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::logic<1>(0x0ULL), vl::rd(mastermacro_rw));
    testdouts_0[60].write(v_.bit(0).sl());
    testdouts_1[60].write(v_.bit(1).sl());
    testdouts_2[60].write(v_.bit(2).sl());
    testdouts_3[60].write(v_.bit(3).sl());
    testdouts_4[60].write(v_.bit(4).sl());
    testdouts_5[60].write(v_.bit(5).sl());
    testdouts_6[60].write(v_.bit(6).sl());
    testdouts_7[60].write(v_.bit(7).sl());
}

// %%CREG RD_RX_DUAL_PHY_0 - RX dual PHY observability
// OFFSET: 0x148
// ATTR: READ, CUST
// FIELDS:
// [7] rxclk_rxhs_rx_en_in - Input rxclk_rxhs_rx_en_in of slave PHY should be connected to output rxclk_rxhs_rx_en_out of master PHY. Input rxclk_rxhs_rx_en_in of master PHY must be tied to 1'b0.
// RESET: 0x00
// [6] rxclk_rxhs_rx_en_out - Output rxclk_rxhs_rx_en_out of master PHY should be connected to input rxclk_rxhs_rx_en_out of master PHY. Output rxclk_rxhs_rx_en_out of slave PHY should be left floating.
// RESET: 0x00
// [5] rxclk_rxhs_rx_pon_in - Input rxclk_rxhs_rx_pon_in of slave PHY should be connected to output rxclk_rxhs_rx_pon_out of master PHY. Input rxclk_rxhs_rx_pon_in of master PHY must be tied to 1'b0.
// RESET: 0x00
// [4] rxclk_rxhs_rx_pon_out - Output rxclk_rxhs_rx_pon_out of master PHY should be connected to input rxclk_rxhs_rx_pon_out of master PHY. Output rxclk_rxhs_rx_pon_out of slave PHY should be left floating.
// RESET: 0x00
// [3] rxclk_rxhs_rx_enlogic_in - Input rxclk_rxhs_rx_enlogic_in of slave PHY should be connected to output rxclk_rxhs_rx_enlogic_out of master PHY. Input rxclk_rxhs_rx_enlogic_in of master PHY must be tied to 1'b0.
// RESET: 0x00
// [2] rxclk_rxhs_rx_enlogic_out - Output rxclk_rxhs_rx_enlogic_out of master PHY should be connected to input rxclk_rxhs_rx_enlogic_out of master PHY. Output rxclk_rxhs_rx_enlogic_out of slave PHY should be left floating.
// RESET: 0x00
// [1] rxclk_rxhs_clk_en_in - Input rxclk_rxhs_clk_en_in of slave PHY should be connected to output rxclk_rxhs_clk_en_out of master PHY. Input rxclk_rxhs_clk_en_in of master PHY must be tied to 1'b0.
// RESET: 0x00
// [0] rxclk_rxhs_clk_en_out - Output rxclk_rxhs_clk_en_out of master PHY should be connected to input rxclk_rxhs_clk_en_out of master PHY. Output rxclk_rxhs_clk_en_out of slave PHY should be left floating.
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_62() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x148ULL))) & vl::cat(vl::rd(rxclk_rxhs_rx_en_in), vl::rd(rxclk_rxhs_rx_en_out), vl::rd(rxclk_rxhs_rx_pon_in), vl::rd(rxclk_rxhs_rx_pon_out), vl::rd(rxclk_rxhs_rx_enlogic_in), vl::rd(rxclk_rxhs_rx_enlogic_out), vl::rd(rxclk_rxhs_clk_en_in), vl::rd(rxclk_rxhs_clk_en_out));
    testdouts_0[61].write(v_.bit(0).sl());
    testdouts_1[61].write(v_.bit(1).sl());
    testdouts_2[61].write(v_.bit(2).sl());
    testdouts_3[61].write(v_.bit(3).sl());
    testdouts_4[61].write(v_.bit(4).sl());
    testdouts_5[61].write(v_.bit(5).sl());
    testdouts_6[61].write(v_.bit(6).sl());
    testdouts_7[61].write(v_.bit(7).sl());
}

// %%CREG RD_RX_DUAL_PHY_1 - RX dual PHY observability
// OFFSET: 0x149
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] rxclk_rxhs_feed_int_clk_in - Input rxclk_rxhs_feed_int_clk_in of slave PHY should be connected to output rxclk_rxhs_feed_int_clk_out of master PHY. Input rxclk_rxhs_feed_int_clk_in of master PHY must be tied to 1'b0.
// RESET: 0x00
// [5] rxclk_rxhs_feed_int_clk_out - Output rxclk_rxhs_feed_int_clk_out of master PHY should be connected to input rxclk_rxhs_feed_int_clk_out of master PHY. Output rxclk_rxhs_feed_int_clk_out of slave PHY should be left floating.
// RESET: 0x00
// [4:3] rx_rxhs_prog_in__1__0__ - Input rx_rxhs_prog_in of slave PHY should be connected to output rx_rxhs_prog_out of master PHY. Input rx_rxhs_prog_in of master PHY must be tied to 1'b0.
// RESET: 0x00
// [2:1] rx_rxhs_prog_out__1__0__ - Output rx_rxhs_prog_out of master PHY should be connected to input rx_rxhs_prog_out of master PHY. Output rx_rxhs_prog_out of slave PHY should be left floating.
// RESET: 0x00
// [0] ulpsactivenotclk_in - Input ulpsactivenotclk_in of slave PHY should be connected to output ulpsactivenotclk of master PHY. Input ulpsactivenotclk_in of master PHY must be tied to 1'b0.
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_63() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x149ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rxclk_rxhs_feed_int_clk_in), vl::rd(rxclk_rxhs_feed_int_clk_out), vl::rd(rx_rxhs_prog_in), vl::rd(rx_rxhs_prog_out), vl::rd(ulpsactivenotclk_in));
    testdouts_0[62].write(v_.bit(0).sl());
    testdouts_1[62].write(v_.bit(1).sl());
    testdouts_2[62].write(v_.bit(2).sl());
    testdouts_3[62].write(v_.bit(3).sl());
    testdouts_4[62].write(v_.bit(4).sl());
    testdouts_5[62].write(v_.bit(5).sl());
    testdouts_6[62].write(v_.bit(6).sl());
    testdouts_7[62].write(v_.bit(7).sl());
}

// %%CREG RD_RX_DUAL_PHY_2 - RX dual PHY observability
// OFFSET: 0x14A
// ATTR: READ, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] launch_des_en_in - Input launch_des_en_in of slave PHY should be connected to output launch_des_en_out of master PHY. Input launch_des_en_in of master PHY must be tied to 1'b0.
// RESET: 0x00
// [0] launch_des_en_out - Output launch_des_en_out of master PHY should be connected to input launch_des_en_out of master PHY. Output launch_des_en_out of slave PHY should be left floating.
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_64() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x14aULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(launch_des_en_in), vl::rd(launch_des_en_out));
    testdouts_0[63].write(v_.bit(0).sl());
    testdouts_1[63].write(v_.bit(1).sl());
    testdouts_2[63].write(v_.bit(2).sl());
    testdouts_3[63].write(v_.bit(3).sl());
    testdouts_4[63].write(v_.bit(4).sl());
    testdouts_5[63].write(v_.bit(5).sl());
    testdouts_6[63].write(v_.bit(6).sl());
    testdouts_7[63].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_0 - PLL control
// OFFSET: 0x15D
// ATTR: RW, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] lb_pll_bustie_if_rw - Ties AFE<->PLL analog interface when PLL is not used (ties down internal nodes that are driven by PLL if present)
// RESET: 0x00
// [5] pll_boot_from_start_rw - PLL boot control comes from main FSM and not BIST machine. Allows BISt testing in other macros that not the one controlling the PLL
// RESET: 0x00
// [4] pll_clksel_en_ovr_rw - PLL clock selection enable override, if 1'b0 and override signal is 1'b1, PLL output clocks are gated
// RESET: 0x00
// [3] pll_clksel_en_ovr_en_rw - PLL clock selection enable override enable
// RESET: 0x00
// [2] pll_atb_sense_sel_rw - PLL analog test bus selection
// RESET: 0x00
// [1] onpll_ovr_rw - PLL power-up enable override
// RESET: 0x00
// [0] onpll_ovr_en_rw - Enable for override of PLL power-up enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_65() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x15dULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(lb_pll_bustie_if_rw), vl::rd(pll_boot_from_start_rw), vl::rd(pll_clksel_en_ovr_rw), vl::rd(pll_clksel_en_ovr_en_rw), vl::rd(pll_atb_sense_sel_rw), vl::rd(onpll_ovr_rw), vl::rd(onpll_ovr_en_rw));
    testdouts_0[64].write(v_.bit(0).sl());
    testdouts_1[64].write(v_.bit(1).sl());
    testdouts_2[64].write(v_.bit(2).sl());
    testdouts_3[64].write(v_.bit(3).sl());
    testdouts_4[64].write(v_.bit(4).sl());
    testdouts_5[64].write(v_.bit(5).sl());
    testdouts_6[64].write(v_.bit(6).sl());
    testdouts_7[64].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_1 - PLL control
// OFFSET: 0x15E
// ATTR: RW, CUST
// FIELDS:
// [7] pll_force_fsm_state_rw - Force state of PLL power-up FSM
// RESET: 0x00
// [6:0] pll_cpbias_cntrl_rw__6__0__ - PLL Charge Pump Bias Control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_66() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x15eULL))) & vl::cat(vl::rd(pll_force_fsm_state_rw), vl::rd(pll_cpbias_cntrl_rw));
    testdouts_0[65].write(v_.bit(0).sl());
    testdouts_1[65].write(v_.bit(1).sl());
    testdouts_2[65].write(v_.bit(2).sl());
    testdouts_3[65].write(v_.bit(3).sl());
    testdouts_4[65].write(v_.bit(4).sl());
    testdouts_5[65].write(v_.bit(5).sl());
    testdouts_6[65].write(v_.bit(6).sl());
    testdouts_7[65].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_2 - PLL control
// OFFSET: 0x15F
// ATTR: RW, CUST
// FIELDS:
// [7:3] reserved_0 - Reserved field
// RESET: 0x00
// [2] pll_gear_shift_ovr_rw - PLL gear shift override
// RESET: 0x00
// [1] pll_gear_shift_ovr_en_rw - PLL gear shift override enable control
// RESET: 0x00
// [0] pll_force_lock_rw -
// NOTES: PLL force lock control:
// <br>1 : PLL lock indication forced
// <br>0 : according to lock detector
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_67() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x15fULL))) & vl::cat(vl::logic<5>(0x0ULL), vl::rd(pll_gear_shift_ovr_rw), vl::rd(pll_gear_shift_ovr_en_rw), vl::rd(pll_force_lock_rw));
    testdouts_0[66].write(v_.bit(0).sl());
    testdouts_1[66].write(v_.bit(1).sl());
    testdouts_2[66].write(v_.bit(2).sl());
    testdouts_3[66].write(v_.bit(3).sl());
    testdouts_4[66].write(v_.bit(4).sl());
    testdouts_5[66].write(v_.bit(5).sl());
    testdouts_6[66].write(v_.bit(6).sl());
    testdouts_7[66].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_3 - PLL control
// OFFSET: 0x160
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_gear_shift_time_rw__7__0__ - PLL gear shift time
// RESET: 0x83
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_68() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x160ULL))) & vl::rd(pll_gear_shift_time_rw).slice<8>(0);
    testdouts_0[67].write(v_.bit(0).sl());
    testdouts_1[67].write(v_.bit(1).sl());
    testdouts_2[67].write(v_.bit(2).sl());
    testdouts_3[67].write(v_.bit(3).sl());
    testdouts_4[67].write(v_.bit(4).sl());
    testdouts_5[67].write(v_.bit(5).sl());
    testdouts_6[67].write(v_.bit(6).sl());
    testdouts_7[67].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_4 - PLL control
// OFFSET: 0x161
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] pll_gear_shift_time_rw__8__ - PLL gear shift time
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_69() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x161ULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(pll_gear_shift_time_rw).bit(8));
    testdouts_0[68].write(v_.bit(0).sl());
    testdouts_1[68].write(v_.bit(1).sl());
    testdouts_2[68].write(v_.bit(2).sl());
    testdouts_3[68].write(v_.bit(3).sl());
    testdouts_4[68].write(v_.bit(4).sl());
    testdouts_5[68].write(v_.bit(5).sl());
    testdouts_6[68].write(v_.bit(6).sl());
    testdouts_7[68].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_5 - PLL control
// OFFSET: 0x162
// ATTR: RW, CUST
// FIELDS:
// [7:2] pll_int_cntrl_rw__5__0__ - PLL Integral Charge Pump control
// RESET: 0x00
// [1:0] pll_gmp_cntrl_rw__1__0__ - PLL GMP Control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_70() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x162ULL))) & vl::cat(vl::rd(pll_int_cntrl_rw), vl::rd(pll_gmp_cntrl_rw));
    testdouts_0[69].write(v_.bit(0).sl());
    testdouts_1[69].write(v_.bit(1).sl());
    testdouts_2[69].write(v_.bit(2).sl());
    testdouts_3[69].write(v_.bit(3).sl());
    testdouts_4[69].write(v_.bit(4).sl());
    testdouts_5[69].write(v_.bit(5).sl());
    testdouts_6[69].write(v_.bit(6).sl());
    testdouts_7[69].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_6 - PLL control
// OFFSET: 0x163
// ATTR: RW, CUST
// FIELDS:
// [7:3] reserved_0 - Reserved field
// RESET: 0x00
// [2] pll_lock_det_on_ovr_rw - PLL lock detector enable override
// RESET: 0x00
// [1] pll_lock_det_on_ovr_en_rw - PLL lock detector enable override enable
// RESET: 0x00
// [0] pll_lockdet_mode_rw - PLL lock detector mode control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_71() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x163ULL))) & vl::cat(vl::logic<5>(0x0ULL), vl::rd(pll_lock_det_on_ovr_rw), vl::rd(pll_lock_det_on_ovr_en_rw), vl::rd(pll_lockdet_mode_rw));
    testdouts_0[70].write(v_.bit(0).sl());
    testdouts_1[70].write(v_.bit(1).sl());
    testdouts_2[70].write(v_.bit(2).sl());
    testdouts_3[70].write(v_.bit(3).sl());
    testdouts_4[70].write(v_.bit(4).sl());
    testdouts_5[70].write(v_.bit(5).sl());
    testdouts_6[70].write(v_.bit(6).sl());
    testdouts_7[70].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_7 - PLL control
// OFFSET: 0x164
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_lock_det_time_rw__7__0__ - PLL lock detector time
// RESET: 0x3F
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_72() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x164ULL))) & vl::rd(pll_lock_det_time_rw).slice<8>(0);
    testdouts_0[71].write(v_.bit(0).sl());
    testdouts_1[71].write(v_.bit(1).sl());
    testdouts_2[71].write(v_.bit(2).sl());
    testdouts_3[71].write(v_.bit(3).sl());
    testdouts_4[71].write(v_.bit(4).sl());
    testdouts_5[71].write(v_.bit(5).sl());
    testdouts_6[71].write(v_.bit(6).sl());
    testdouts_7[71].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_8 - PLL control
// OFFSET: 0x165
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] pll_lock_det_time_rw__8__ - PLL lock detector time
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_73() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x165ULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(pll_lock_det_time_rw).bit(8));
    testdouts_0[72].write(v_.bit(0).sl());
    testdouts_1[72].write(v_.bit(1).sl());
    testdouts_2[72].write(v_.bit(2).sl());
    testdouts_3[72].write(v_.bit(3).sl());
    testdouts_4[72].write(v_.bit(4).sl());
    testdouts_5[72].write(v_.bit(5).sl());
    testdouts_6[72].write(v_.bit(6).sl());
    testdouts_7[72].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_9 - PLL control
// OFFSET: 0x166
// ATTR: RW, CUST
// FIELDS:
// [7:3] reserved_0 - Reserved field
// RESET: 0x00
// [2] pll_lock_sel_rw - PLL lock selection, relock detector mode
// RESET: 0x00
// [1] pll_lock_ovr_rw - PLL lock override
// RESET: 0x00
// [0] pll_lock_ovr_en_rw - PLL lock override enable control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_74() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x166ULL))) & vl::cat(vl::logic<5>(0x0ULL), vl::rd(pll_lock_sel_rw), vl::rd(pll_lock_ovr_rw), vl::rd(pll_lock_ovr_en_rw));
    testdouts_0[73].write(v_.bit(0).sl());
    testdouts_1[73].write(v_.bit(1).sl());
    testdouts_2[73].write(v_.bit(2).sl());
    testdouts_3[73].write(v_.bit(3).sl());
    testdouts_4[73].write(v_.bit(4).sl());
    testdouts_5[73].write(v_.bit(5).sl());
    testdouts_6[73].write(v_.bit(6).sl());
    testdouts_7[73].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_10 - PLL control
// OFFSET: 0x167
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_meas_iv_rw__7__0__ -
// NOTES: PLL internal nodes selection for analog test bus
// <br>mpll_meas_iv<0> : connects the internal positive DCC control line (vcip/vcqp : depend on mpll_meas_iv<11>) to the analog test bus.
// <br>mpll_meas_iv<1> : connects the internal negative DCC control line (vcim/vcqm : depend on mpll_meas_iv<11>) to the analog test bus.
// <br>mpll_meas_iv<2> : connects internal supply voltage of the VCO (ivco) to the analog test bus.
// <br>mpll_meas_iv<3> : connects vreg_mpll voltage to the analog test bus.
// <br>mpll_meas_iv<4> : connects vp_cp voltage to the analog test bus.
// <br>mpll_meas_iv<5> : connects reference voltage of charge pump regulator to the analog test bus(depend on mpll_prg<7:6>; default vp voltage).
// <br>mpll_meas_iv<6> : connects current source 'icopy'(1/128 of vco current) to the analog test bus.
// <br>mpll_meas_iv<7> : connects gd to the analog test bus.
// <br>mpll_meas_iv<8> : connects vcntrl voltage to the analog test bus.
// <br>mpll_meas_iv<9> : connects vref voltage to the analog test bus.
// <br>mpll_meas_iv<10> : connects vpsf voltage to the analog test bus.
// <br>mpll_meas_iv<11> : chooses which DCC control lines are measured 1'b, vcip/vcim 0'b, vcqp/vcqm (depend on mpll_meas_iv<0>,mpll_meas_iv<1>)
// <br>mpll_meas_iv<12> : connects vreg_pmix voltage to the analog test bus.
// <br>mpll_meas_iv<13> : connects vbg voltage (reference for vreg_pmix regulator from bandgap) to the analog test bus.
// <br>mpll_meas_iv<14> : connects vmb (vmaster_bias) voltage to the analog test bus.
// <br>mpll_meas_iv<15> : connects vbf_sf voltage to the analog test bus.
// <br>mpll_meas_iv<16> : not used
// <br>mpll_meas_iv<17> : not used
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_75() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x167ULL))) & vl::rd(pll_meas_iv_rw).slice<8>(0);
    testdouts_0[74].write(v_.bit(0).sl());
    testdouts_1[74].write(v_.bit(1).sl());
    testdouts_2[74].write(v_.bit(2).sl());
    testdouts_3[74].write(v_.bit(3).sl());
    testdouts_4[74].write(v_.bit(4).sl());
    testdouts_5[74].write(v_.bit(5).sl());
    testdouts_6[74].write(v_.bit(6).sl());
    testdouts_7[74].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_11 - PLL control
// OFFSET: 0x168
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_meas_iv_rw__15__8__ -
// NOTES: PLL internal nodes selection for analog test bus
// <br>mpll_meas_iv<0> : connects the internal positive DCC control line (vcip/vcqp : depend on mpll_meas_iv<11>) to the analog test bus.
// <br>mpll_meas_iv<1> : connects the internal negative DCC control line (vcim/vcqm : depend on mpll_meas_iv<11>) to the analog test bus.
// <br>mpll_meas_iv<2> : connects internal supply voltage of the VCO (ivco) to the analog test bus.
// <br>mpll_meas_iv<3> : connects vreg_mpll voltage to the analog test bus.
// <br>mpll_meas_iv<4> : connects vp_cp voltage to the analog test bus.
// <br>mpll_meas_iv<5> : connects reference voltage of charge pump regulator to the analog test bus(depend on mpll_prg<7:6>; default vp voltage).
// <br>mpll_meas_iv<6> : connects current source 'icopy'(1/128 of vco current) to the analog test bus.
// <br>mpll_meas_iv<7> : connects gd to the analog test bus.
// <br>mpll_meas_iv<8> : connects vcntrl voltage to the analog test bus.
// <br>mpll_meas_iv<9> : connects vref voltage to the analog test bus.
// <br>mpll_meas_iv<10> : connects vpsf voltage to the analog test bus.
// <br>mpll_meas_iv<11> : chooses which DCC control lines are measured 1'b, vcip/vcim 0'b, vcqp/vcqm (depend on mpll_meas_iv<0>,mpll_meas_iv<1>)
// <br>mpll_meas_iv<12> : connects vreg_pmix voltage to the analog test bus.
// <br>mpll_meas_iv<13> : connects vbg voltage (reference for vreg_pmix regulator from bandgap) to the analog test bus.
// <br>mpll_meas_iv<14> : connects vmb (vmaster_bias) voltage to the analog test bus.
// <br>mpll_meas_iv<15> : connects vbf_sf voltage to the analog test bus.
// <br>mpll_meas_iv<16> : not used
// <br>mpll_meas_iv<17> : not used
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_76() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x168ULL))) & vl::rd(pll_meas_iv_rw).slice<8>(8);
    testdouts_0[75].write(v_.bit(0).sl());
    testdouts_1[75].write(v_.bit(1).sl());
    testdouts_2[75].write(v_.bit(2).sl());
    testdouts_3[75].write(v_.bit(3).sl());
    testdouts_4[75].write(v_.bit(4).sl());
    testdouts_5[75].write(v_.bit(5).sl());
    testdouts_6[75].write(v_.bit(6).sl());
    testdouts_7[75].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_12 - PLL control
// OFFSET: 0x169
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] pll_meas_iv_rw__17__16__ -
// NOTES: PLL internal nodes selection for analog test bus
// <br>mpll_meas_iv<0> : connects the internal positive DCC control line (vcip/vcqp : depend on mpll_meas_iv<11>) to the analog test bus.
// <br>mpll_meas_iv<1> : connects the internal negative DCC control line (vcim/vcqm : depend on mpll_meas_iv<11>) to the analog test bus.
// <br>mpll_meas_iv<2> : connects internal supply voltage of the VCO (ivco) to the analog test bus.
// <br>mpll_meas_iv<3> : connects vreg_mpll voltage to the analog test bus.
// <br>mpll_meas_iv<4> : connects vp_cp voltage to the analog test bus.
// <br>mpll_meas_iv<5> : connects reference voltage of charge pump regulator to the analog test bus(depend on mpll_prg<7:6>; default vp voltage).
// <br>mpll_meas_iv<6> : connects current source 'icopy'(1/128 of vco current) to the analog test bus.
// <br>mpll_meas_iv<7> : connects gd to the analog test bus.
// <br>mpll_meas_iv<8> : connects vcntrl voltage to the analog test bus.
// <br>mpll_meas_iv<9> : connects vref voltage to the analog test bus.
// <br>mpll_meas_iv<10> : connects vpsf voltage to the analog test bus.
// <br>mpll_meas_iv<11> : chooses which DCC control lines are measured 1'b, vcip/vcim 0'b, vcqp/vcqm (depend on mpll_meas_iv<0>,mpll_meas_iv<1>)
// <br>mpll_meas_iv<12> : connects vreg_pmix voltage to the analog test bus.
// <br>mpll_meas_iv<13> : connects vbg voltage (reference for vreg_pmix regulator from bandgap) to the analog test bus.
// <br>mpll_meas_iv<14> : connects vmb (vmaster_bias) voltage to the analog test bus.
// <br>mpll_meas_iv<15> : connects vbf_sf voltage to the analog test bus.
// <br>mpll_meas_iv<16> : not used
// <br>mpll_meas_iv<17> : not used
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_77() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x169ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(pll_meas_iv_rw).slice<2>(16));
    testdouts_0[76].write(v_.bit(0).sl());
    testdouts_1[76].write(v_.bit(1).sl());
    testdouts_2[76].write(v_.bit(2).sl());
    testdouts_3[76].write(v_.bit(3).sl());
    testdouts_4[76].write(v_.bit(4).sl());
    testdouts_5[76].write(v_.bit(5).sl());
    testdouts_6[76].write(v_.bit(6).sl());
    testdouts_7[76].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_13 - PLL control
// OFFSET: 0x16A
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_mpll_prog_rw__7__0__ -
// NOTES: PLL Analog Programmability Control
// <br>mpll_prg<0> when 1 bypass internal regulator mpll_vreg_ckout
// <br>mpll_prg<1> when 1 bypass internal regulator mpll_vreg1p2
// <br>mpll_prg<2> when 1 all additional capacitors are connected to vcntrl
// <br>mpll_prg<3> when 1 all additional capacitors are disconnected from vcntrl
// <br>mpll_prg<4> connects mpll_vcntr_cap block, extra capacitances for vcntrl net
// <br>mpll_prg<9:5> not used
// <br>mpll_prg<11:10> change the voltage reference for the charge pump regulator between
// <br>'00' - filtered vp,
// <br>'10' - doubled bg voltage,
// <br>'10' -  bg voltage,
// <br>'11' - atb bus
// <br>mpll_prg<13:12> change the voltage reference for the mpll_vreg_ckout regulator between
// <br>'00' : filtered vp
// <br>'01' : doubled bg voltage
// <br>'10' : bg voltage,
// <br>'11' : atb bus
// <br>mpll_prg<15:14> change the voltage reference for the pmix regulator between
// <br>'00' - filtered vp,
// <br>'01' - doubled bg voltage,
// <br>'10' - bg voltage,
// <br>'11' - atb bus
// <br>mpll_prg<16> - not used
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_78() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x16aULL))) & vl::rd(pll_mpll_prog_rw).slice<8>(0);
    testdouts_0[77].write(v_.bit(0).sl());
    testdouts_1[77].write(v_.bit(1).sl());
    testdouts_2[77].write(v_.bit(2).sl());
    testdouts_3[77].write(v_.bit(3).sl());
    testdouts_4[77].write(v_.bit(4).sl());
    testdouts_5[77].write(v_.bit(5).sl());
    testdouts_6[77].write(v_.bit(6).sl());
    testdouts_7[77].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_14 - PLL control
// OFFSET: 0x16B
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_mpll_prog_rw__15__8__ -
// NOTES: PLL Analog Programmability Control
// <br>mpll_prg<0> when 1 bypass internal regulator mpll_vreg_ckout
// <br>mpll_prg<1> when 1 bypass internal regulator mpll_vreg1p2
// <br>mpll_prg<2> when 1 all additional capacitors are connected to vcntrl
// <br>mpll_prg<3> when 1 all additional capacitors are disconnected from vcntrl
// <br>mpll_prg<4> connects mpll_vcntr_cap block, extra capacitances for vcntrl net
// <br>mpll_prg<9:5> not used
// <br>mpll_prg<11:10> change the voltage reference for the charge pump regulator between
// <br>'00' - filtered vp,
// <br>'10' - doubled bg voltage,
// <br>'10' - bg voltage,
// <br>'11' - atb bus
// <br>mpll_prg<13:12> change the voltage reference for the mpll_vreg_ckout regulator between
// <br>'00' - filtered vp
// <br>'01' - doubled bg voltage
// <br>'10' - bg voltage,
// <br>'11' - atb bus
// <br>mpll_prg<15:14> change the voltage reference for the pmix regulator between
// <br>'00' - filtered vp,
// <br>'01' - doubled bg voltage,
// <br>'10' - bg voltage,
// <br>'11' - atb bus
// <br>mpll_prg<16> - not used
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_79() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x16bULL))) & vl::rd(pll_mpll_prog_rw).slice<8>(8);
    testdouts_0[78].write(v_.bit(0).sl());
    testdouts_1[78].write(v_.bit(1).sl());
    testdouts_2[78].write(v_.bit(2).sl());
    testdouts_3[78].write(v_.bit(3).sl());
    testdouts_4[78].write(v_.bit(4).sl());
    testdouts_5[78].write(v_.bit(5).sl());
    testdouts_6[78].write(v_.bit(6).sl());
    testdouts_7[78].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_15 - PLL control
// OFFSET: 0x16C
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] pll_mpll_prog_rw__16__ -
// NOTES: PLL Analog Programmability Control
// <br>mpll_prg<0> when 1 bypass internal regulator mpll_vreg_ckout
// <br>mpll_prg<1> when 1 bypass internal regulator mpll_vreg1p2
// <br>mpll_prg<2> when 1 all additional capacitors are connected to vcntrl
// <br>mpll_prg<3> when 1 all additional capacitors are disconnected from vcntrl
// <br>mpll_prg<4> connects mpll_vcntr_cap block, extra capacitances for vcntrl net
// <br>mpll_prg<9:5> not used
// <br>mpll_prg<11:10> change the voltage reference for the charge pump regulator between
// <br>'00' - filtered vp,
// <br>'10' - doubled bg voltage,
// <br>'10' - bg voltage,
// <br>'11' - atb bus
// <br>mpll_prg<13:12> change the voltage reference for the mpll_vreg_ckout regulator between
// <br>'00' - filtered vp
// <br>'01' - doubled bg voltage
// <br>'10' - bg voltage,
// <br>'11' - atb bus
// <br>mpll_prg<15:14> change the voltage reference for the pmix regulator between
// <br>'00' - filtered vp,
// <br>'01' - doubled bg voltage,
// <br>'10' - bg voltage,
// <br>'11' - atb bus
// <br>mpll_prg<16> - not used
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_80() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x16cULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(pll_mpll_prog_rw).bit(16));
    testdouts_0[79].write(v_.bit(0).sl());
    testdouts_1[79].write(v_.bit(1).sl());
    testdouts_2[79].write(v_.bit(2).sl());
    testdouts_3[79].write(v_.bit(3).sl());
    testdouts_4[79].write(v_.bit(4).sl());
    testdouts_5[79].write(v_.bit(5).sl());
    testdouts_6[79].write(v_.bit(6).sl());
    testdouts_7[79].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_16 - PLL control
// OFFSET: 0x16D
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4:0] pll_opmode_rw__4__0__ - Reserved
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_81() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x16dULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(pll_opmode_rw));
    testdouts_0[80].write(v_.bit(0).sl());
    testdouts_1[80].write(v_.bit(1).sl());
    testdouts_2[80].write(v_.bit(2).sl());
    testdouts_3[80].write(v_.bit(3).sl());
    testdouts_4[80].write(v_.bit(4).sl());
    testdouts_5[80].write(v_.bit(5).sl());
    testdouts_6[80].write(v_.bit(6).sl());
    testdouts_7[80].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_17 - PLL control
// OFFSET: 0x16E
// ATTR: RW, CUST
// FIELDS:
// [7] pll_pwron_ovr_rw - PLL poweron override
// RESET: 0x00
// [6] pll_pwron_ovr_en_rw - PLL poweron override enable control
// RESET: 0x00
// [5:0] pll_prop_cntrl_rw__5__0__ - PLL Proportional Charge Pump control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_82() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x16eULL))) & vl::cat(vl::rd(pll_pwron_ovr_rw), vl::rd(pll_pwron_ovr_en_rw), vl::rd(pll_prop_cntrl_rw));
    testdouts_0[81].write(v_.bit(0).sl());
    testdouts_1[81].write(v_.bit(1).sl());
    testdouts_2[81].write(v_.bit(2).sl());
    testdouts_3[81].write(v_.bit(3).sl());
    testdouts_4[81].write(v_.bit(4).sl());
    testdouts_5[81].write(v_.bit(5).sl());
    testdouts_6[81].write(v_.bit(6).sl());
    testdouts_7[81].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_18 - PLL control
// OFFSET: 0x16F
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] pll_reset_ovr_rw - PLL reset override
// RESET: 0x00
// [0] pll_reset_ovr_en_rw - PLL reset override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_83() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x16fULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(pll_reset_ovr_rw), vl::rd(pll_reset_ovr_en_rw));
    testdouts_0[82].write(v_.bit(0).sl());
    testdouts_1[82].write(v_.bit(1).sl());
    testdouts_2[82].write(v_.bit(2).sl());
    testdouts_3[82].write(v_.bit(3).sl());
    testdouts_4[82].write(v_.bit(4).sl());
    testdouts_5[82].write(v_.bit(5).sl());
    testdouts_6[82].write(v_.bit(6).sl());
    testdouts_7[82].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_19 - PLL control
// OFFSET: 0x170
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_rst_time_rw__7__0__ - PLL reset time
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_84() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x170ULL))) & vl::rd(pll_rst_time_rw).slice<8>(0);
    testdouts_0[83].write(v_.bit(0).sl());
    testdouts_1[83].write(v_.bit(1).sl());
    testdouts_2[83].write(v_.bit(2).sl());
    testdouts_3[83].write(v_.bit(3).sl());
    testdouts_4[83].write(v_.bit(4).sl());
    testdouts_5[83].write(v_.bit(5).sl());
    testdouts_6[83].write(v_.bit(6).sl());
    testdouts_7[83].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_20 - PLL control
// OFFSET: 0x171
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] pll_rst_time_rw__8__ - PLL reset time
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_85() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x171ULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(pll_rst_time_rw).bit(8));
    testdouts_0[84].write(v_.bit(0).sl());
    testdouts_1[84].write(v_.bit(1).sl());
    testdouts_2[84].write(v_.bit(2).sl());
    testdouts_3[84].write(v_.bit(3).sl());
    testdouts_4[84].write(v_.bit(4).sl());
    testdouts_5[84].write(v_.bit(5).sl());
    testdouts_6[84].write(v_.bit(6).sl());
    testdouts_7[84].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_21 - PLL control
// OFFSET: 0x172
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] pll_startup_fsm_state_rw__3__0__ - PLL FSM state
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_86() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x172ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(pll_startup_fsm_state_rw));
    testdouts_0[85].write(v_.bit(0).sl());
    testdouts_1[85].write(v_.bit(1).sl());
    testdouts_2[85].write(v_.bit(2).sl());
    testdouts_3[85].write(v_.bit(3).sl());
    testdouts_4[85].write(v_.bit(4).sl());
    testdouts_5[85].write(v_.bit(5).sl());
    testdouts_6[85].write(v_.bit(6).sl());
    testdouts_7[85].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_22 - PLL control
// OFFSET: 0x173
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_th1_rw__7__0__ - PLL phase error threshold
// RESET: 0x03
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_87() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x173ULL))) & vl::rd(pll_th1_rw).slice<8>(0);
    testdouts_0[86].write(v_.bit(0).sl());
    testdouts_1[86].write(v_.bit(1).sl());
    testdouts_2[86].write(v_.bit(2).sl());
    testdouts_3[86].write(v_.bit(3).sl());
    testdouts_4[86].write(v_.bit(4).sl());
    testdouts_5[86].write(v_.bit(5).sl());
    testdouts_6[86].write(v_.bit(6).sl());
    testdouts_7[86].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_23 - PLL control
// OFFSET: 0x174
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] pll_th1_rw__9__8__ - PLL phase error threshold
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_88() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x174ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(pll_th1_rw).slice<2>(8));
    testdouts_0[87].write(v_.bit(0).sl());
    testdouts_1[87].write(v_.bit(1).sl());
    testdouts_2[87].write(v_.bit(2).sl());
    testdouts_3[87].write(v_.bit(3).sl());
    testdouts_4[87].write(v_.bit(4).sl());
    testdouts_5[87].write(v_.bit(5).sl());
    testdouts_6[87].write(v_.bit(6).sl());
    testdouts_7[87].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_24 - PLL control
// OFFSET: 0x175
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_th2_rw__7__0__ - PLL lock counter
// RESET: 0x2D
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_89() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x175ULL))) & vl::rd(pll_th2_rw);
    testdouts_0[88].write(v_.bit(0).sl());
    testdouts_1[88].write(v_.bit(1).sl());
    testdouts_2[88].write(v_.bit(2).sl());
    testdouts_3[88].write(v_.bit(3).sl());
    testdouts_4[88].write(v_.bit(4).sl());
    testdouts_5[88].write(v_.bit(5).sl());
    testdouts_6[88].write(v_.bit(6).sl());
    testdouts_7[88].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_25 - PLL control
// OFFSET: 0x176
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_th3_rw__7__0__ - PLL unlock counter
// RESET: 0x03
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_90() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x176ULL))) & vl::rd(pll_th3_rw);
    testdouts_0[89].write(v_.bit(0).sl());
    testdouts_1[89].write(v_.bit(1).sl());
    testdouts_2[89].write(v_.bit(2).sl());
    testdouts_3[89].write(v_.bit(3).sl());
    testdouts_4[89].write(v_.bit(4).sl());
    testdouts_5[89].write(v_.bit(5).sl());
    testdouts_6[89].write(v_.bit(6).sl());
    testdouts_7[89].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_26 - PLL control
// OFFSET: 0x177
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5:0] pll_th_delay_rw__5__0__ - PLL Programmable delay for lock indication after frequency change (relock)
// RESET: 0x18
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_91() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x177ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(pll_th_delay_rw));
    testdouts_0[90].write(v_.bit(0).sl());
    testdouts_1[90].write(v_.bit(1).sl());
    testdouts_2[90].write(v_.bit(2).sl());
    testdouts_3[90].write(v_.bit(3).sl());
    testdouts_4[90].write(v_.bit(4).sl());
    testdouts_5[90].write(v_.bit(5).sl());
    testdouts_6[90].write(v_.bit(6).sl());
    testdouts_7[90].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_27 - PLL control
// OFFSET: 0x178
// ATTR: RW, CUST
// FIELDS:
// [7] pll_n_ovr_en_rw - PLL input divider override enable
// RESET: 0x00
// [6:3] pll_n_ovr_rw__3__0__ - PLL input divider override
// RESET: 0x0B
// [2:0] pll_tstplldig_rw__2__0__ -
// NOTES: PLL digital testability signals selection
// <br>000 - lock signal (same as lock output)
// <br>001 - refclk (output of input N divider)
// <br>010 - fbclk (output of feedback M divider)
// <br>011 - int_clkin (clkin gated with onpll to save power)
// <br>100 - lock signal from lock detector
// <br>101 - output from relock delay block
// <br>110 - buf_vcoclk (VCO clock input of digital)
// <br>111 - updatepll signal
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_92() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x178ULL))) & vl::cat(vl::rd(pll_n_ovr_en_rw), vl::rd(pll_n_ovr_rw), vl::rd(pll_tstplldig_rw));
    testdouts_0[91].write(v_.bit(0).sl());
    testdouts_1[91].write(v_.bit(1).sl());
    testdouts_2[91].write(v_.bit(2).sl());
    testdouts_3[91].write(v_.bit(3).sl());
    testdouts_4[91].write(v_.bit(4).sl());
    testdouts_5[91].write(v_.bit(5).sl());
    testdouts_6[91].write(v_.bit(6).sl());
    testdouts_7[91].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_28 - PLL control
// OFFSET: 0x179
// ATTR: RW, CUST
// FIELDS:
// [7:0] pll_m_ovr_rw__7__0__ - PLL feedback divider override
// RESET: 0xF8
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_93() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x179ULL))) & vl::rd(pll_m_ovr_rw).slice<8>(0);
    testdouts_0[92].write(v_.bit(0).sl());
    testdouts_1[92].write(v_.bit(1).sl());
    testdouts_2[92].write(v_.bit(2).sl());
    testdouts_3[92].write(v_.bit(3).sl());
    testdouts_4[92].write(v_.bit(4).sl());
    testdouts_5[92].write(v_.bit(5).sl());
    testdouts_6[92].write(v_.bit(6).sl());
    testdouts_7[92].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_29 - PLL control
// OFFSET: 0x17A
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] pll_m_ovr_rw__9__8__ - PLL feedback divider override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_94() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x17aULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(pll_m_ovr_rw).slice<2>(8));
    testdouts_0[93].write(v_.bit(0).sl());
    testdouts_1[93].write(v_.bit(1).sl());
    testdouts_2[93].write(v_.bit(2).sl());
    testdouts_3[93].write(v_.bit(3).sl());
    testdouts_4[93].write(v_.bit(4).sl());
    testdouts_5[93].write(v_.bit(5).sl());
    testdouts_6[93].write(v_.bit(6).sl());
    testdouts_7[93].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_30 - PLL control
// OFFSET: 0x17B
// ATTR: RW, CUST
// FIELDS:
// [7] pll_vco_cntrl_ovr_en_rw - PLL VCO control override enable
// RESET: 0x00
// [6:1] pll_vco_cntrl_ovr_rw__5__0__ - PLL VCO control override
// RESET: 0x3F
// [0] pll_m_ovr_en_rw - PLL feedback divider override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_95() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x17bULL))) & vl::cat(vl::rd(pll_vco_cntrl_ovr_en_rw), vl::rd(pll_vco_cntrl_ovr_rw), vl::rd(pll_m_ovr_en_rw));
    testdouts_0[94].write(v_.bit(0).sl());
    testdouts_1[94].write(v_.bit(1).sl());
    testdouts_2[94].write(v_.bit(2).sl());
    testdouts_3[94].write(v_.bit(3).sl());
    testdouts_4[94].write(v_.bit(4).sl());
    testdouts_5[94].write(v_.bit(5).sl());
    testdouts_6[94].write(v_.bit(6).sl());
    testdouts_7[94].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_PLL_31 - PLL control
// OFFSET: 0x17C
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] pll_clkouten_right_rw - PLL enable of right side buffers
// RESET: 0x00
// [0] pll_clkouten_left_rw - PLL enable of left side buffers
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_96() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x17cULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(pll_clkouten_right_rw), vl::rd(pll_clkouten_left_rw));
    testdouts_0[95].write(v_.bit(0).sl());
    testdouts_1[95].write(v_.bit(1).sl());
    testdouts_2[95].write(v_.bit(2).sl());
    testdouts_3[95].write(v_.bit(3).sl());
    testdouts_4[95].write(v_.bit(4).sl());
    testdouts_5[95].write(v_.bit(5).sl());
    testdouts_6[95].write(v_.bit(6).sl());
    testdouts_7[95].write(v_.bit(7).sl());
}

// %%CREG RD_RX_PLL_0 - PLL observability
// OFFSET: 0x191
// ATTR: READ, CUST
// FIELDS:
// [7] pll_vpl_det - Supply presence detector
// RESET: 0x00
// [6] pll_clksel_en - PHY internal clksel override (for lane startup sequence)
// RESET: 0x00
// [5] pll_lock - PLL lock observability
// RESET: 0x00
// [4] pll_lock_det_on - PLL lock detector power-on
// RESET: 0x00
// [3] pll_gear_shift - PLL gear shift
// RESET: 0x00
// [2:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] onpll - PLL power-on
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_97() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x191ULL))) & vl::cat(vl::rd(pll_vpl_det), vl::rd(pll_clksel_en), vl::rd(pll_lock), vl::rd(pll_lock_det_on), vl::rd(pll_gear_shift), vl::logic<2>(0x0ULL), vl::rd(onpll));
    testdouts_0[96].write(v_.bit(0).sl());
    testdouts_1[96].write(v_.bit(1).sl());
    testdouts_2[96].write(v_.bit(2).sl());
    testdouts_3[96].write(v_.bit(3).sl());
    testdouts_4[96].write(v_.bit(4).sl());
    testdouts_5[96].write(v_.bit(5).sl());
    testdouts_6[96].write(v_.bit(6).sl());
    testdouts_7[96].write(v_.bit(7).sl());
}

// %%CREG RD_RX_PLL_1 - PLL observability
// OFFSET: 0x192
// ATTR: READ, CUST
// FIELDS:
// [7:0] pll_m__7__0__ - Feedback Multiplication ratio M
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_98() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x192ULL))) & vl::rd(pll_m).slice<8>(0);
    testdouts_0[97].write(v_.bit(0).sl());
    testdouts_1[97].write(v_.bit(1).sl());
    testdouts_2[97].write(v_.bit(2).sl());
    testdouts_3[97].write(v_.bit(3).sl());
    testdouts_4[97].write(v_.bit(4).sl());
    testdouts_5[97].write(v_.bit(5).sl());
    testdouts_6[97].write(v_.bit(6).sl());
    testdouts_7[97].write(v_.bit(7).sl());
}

// %%CREG RD_RX_PLL_2 - PLL observability
// OFFSET: 0x193
// ATTR: READ, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] pll_m__9__8__ - Feedback Multiplication ratio M
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_99() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x193ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(pll_m).slice<2>(8));
    testdouts_0[98].write(v_.bit(0).sl());
    testdouts_1[98].write(v_.bit(1).sl());
    testdouts_2[98].write(v_.bit(2).sl());
    testdouts_3[98].write(v_.bit(3).sl());
    testdouts_4[98].write(v_.bit(4).sl());
    testdouts_5[98].write(v_.bit(5).sl());
    testdouts_6[98].write(v_.bit(6).sl());
    testdouts_7[98].write(v_.bit(7).sl());
}

// %%CREG RD_RX_PLL_3 - PLL observability
// OFFSET: 0x194
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] pll_startup_done - PLL startup done
// RESET: 0x00
// [5] pll_reset - PLL reset
// RESET: 0x00
// [4] pll_pwron - PLL poweron
// RESET: 0x00
// [3:0] pll_n__3__0__ - Input frequency divison ratio
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_100() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x194ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(pll_startup_done), vl::rd(pll_reset), vl::rd(pll_pwron), vl::rd(pll_n));
    testdouts_0[99].write(v_.bit(0).sl());
    testdouts_1[99].write(v_.bit(1).sl());
    testdouts_2[99].write(v_.bit(2).sl());
    testdouts_3[99].write(v_.bit(3).sl());
    testdouts_4[99].write(v_.bit(4).sl());
    testdouts_5[99].write(v_.bit(5).sl());
    testdouts_6[99].write(v_.bit(6).sl());
    testdouts_7[99].write(v_.bit(7).sl());
}

// %%CREG RD_RX_PLL_4 - PLL observability
// OFFSET: 0x195
// ATTR: READ, CUST
// FIELDS:
// [7] pll_lock_int - PLL internal lock indicator
// RESET: 0x00
// [6] pll_testlock - PLL test probe
// RESET: 0x00
// [5:0] pll_vco_cntrl__5__0__ - PLL VCO control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_101() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x195ULL))) & vl::cat(vl::rd(pll_lock_int), vl::rd(pll_testlock), vl::rd(pll_vco_cntrl));
    testdouts_0[100].write(v_.bit(0).sl());
    testdouts_1[100].write(v_.bit(1).sl());
    testdouts_2[100].write(v_.bit(2).sl());
    testdouts_3[100].write(v_.bit(3).sl());
    testdouts_4[100].write(v_.bit(4).sl());
    testdouts_5[100].write(v_.bit(5).sl());
    testdouts_6[100].write(v_.bit(6).sl());
    testdouts_7[100].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CB_0 - Common block control
// OFFSET: 0x1AA
// ATTR: RW, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:5] cb_sel_vrefcd_lprx_rw__1__0__ -
// NOTES: LPTX voltage reference selection:
// <br>0 : 1100mV
// <br>1 : 1200mV
// RESET: 0x01
// [4:2] cb_sel_v400_prog_rw__2__0__ -
// NOTES: Select value of cb_v400
// <br>1XX : 400mV
// <br>011 : 450mV
// <br>010 : 350mV
// <br>001 : 300mV
// <br>000 : 200mV
// RESET: 0x04
// [1] cb_sel_chop_clk_rw -
// NOTES: Select bandgap clock source:
// <br>0 : Internal oscillator (internal clk source)
// <br>1 : External clock signal (cb_chop_clk)
// RESET: 0x01
// [0] cb_chop_clk_en_rw -
// NOTES: Bandgap chop clock enable
// <br>0 : Chop clock gated
// <br>1 : Chop clock switching
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_102() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aaULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(cb_sel_vrefcd_lprx_rw), vl::rd(cb_sel_v400_prog_rw), vl::rd(cb_sel_chop_clk_rw), vl::rd(cb_chop_clk_en_rw));
    testdouts_0[101].write(v_.bit(0).sl());
    testdouts_1[101].write(v_.bit(1).sl());
    testdouts_2[101].write(v_.bit(2).sl());
    testdouts_3[101].write(v_.bit(3).sl());
    testdouts_4[101].write(v_.bit(4).sl());
    testdouts_5[101].write(v_.bit(5).sl());
    testdouts_6[101].write(v_.bit(6).sl());
    testdouts_7[101].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CB_1 - Common block control
// OFFSET: 0x1AB
// ATTR: RW, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] cb_atb_vbe_sel_rw - VBe selection to Analog Test Bus
// RESET: 0x00
// [5:3] txlp_prog_rw__2__0__ - LP-TX programmability
// RESET: 0x00
// [2] cb_sel_vreflptx_prog_rw -
// NOTES: LPRX contention detector voltage reference selection:
// <br>00 : 275mV
// <br>01 : 300mV
// <br>10 : 325mV
// <br>11: 350mV
// RESET: 0x01
// [1:0] cb_sel_vref_lprx_rw__1__0__ -
// NOTES: LPRX voltage reference selection:
// <br>00 : 600mV
// <br>01 : 625mV
// <br>10 : 650mV
// <br>11 : 675mV
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_103() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1abULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(cb_atb_vbe_sel_rw), vl::rd(txlp_prog_rw), vl::rd(cb_sel_vreflptx_prog_rw), vl::rd(cb_sel_vref_lprx_rw));
    testdouts_0[102].write(v_.bit(0).sl());
    testdouts_1[102].write(v_.bit(1).sl());
    testdouts_2[102].write(v_.bit(2).sl());
    testdouts_3[102].write(v_.bit(3).sl());
    testdouts_4[102].write(v_.bit(4).sl());
    testdouts_5[102].write(v_.bit(5).sl());
    testdouts_6[102].write(v_.bit(6).sl());
    testdouts_7[102].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CB_2 - Common block control
// OFFSET: 0x1AC
// ATTR: RW, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] rx_rxlp_bias_prog_rw - LP-RX biasing control
// RESET: 0x00
// [5:4] reserved_1 - Reserved field
// RESET: 0x00
// [3] rx_cb_pre_iref_on_rw - common block reference current control
// RESET: 0x01
// [2] cb_atb_ext_con_rw -
// NOTES: allows direct connecting ATB line into ATB pin:
// <br>0 : disable
// <br>1 : enable
// RESET: 0x00
// [1:0] cb_atb_sel_rw__1__0__ -
// NOTES: Analog Test Bus selection:
// <br>cb_atb_sel[1]:
// <br>0 : ATB LINE (atb_line)
// <br>1 : External resistors
// <br>cb_atb_sel[0]:
// <br>0 : DAC
// <br>1 : TX/RX replica
// RESET: 0x03
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_104() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1acULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx_rxlp_bias_prog_rw), vl::logic<2>(0x0ULL), vl::rd(rx_cb_pre_iref_on_rw), vl::rd(cb_atb_ext_con_rw), vl::rd(cb_atb_sel_rw));
    testdouts_0[103].write(v_.bit(0).sl());
    testdouts_1[103].write(v_.bit(1).sl());
    testdouts_2[103].write(v_.bit(2).sl());
    testdouts_3[103].write(v_.bit(3).sl());
    testdouts_4[103].write(v_.bit(4).sl());
    testdouts_5[103].write(v_.bit(5).sl());
    testdouts_6[103].write(v_.bit(6).sl());
    testdouts_7[103].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CB_3 - Common block control
// OFFSET: 0x1AD
// ATTR: RW, CUST
// FIELDS:
// [7] cb_lpibias_en_ovr_en_rw - Override Enable voltage control to LPTX and currents for LPTX, LPRX
// RESET: 0x00
// [6] cb_bg_en_ovr_rw - Bandgap enable override
// RESET: 0x00
// [5] cb_bg_en_ovr_en_rw - Bandgap enable override
// RESET: 0x00
// [4] cb_atb_comp_clk_ovr_rw - Clock used in analog comparator override
// RESET: 0x00
// [3] cb_atb_comp_clk_ovr_en_rw - Clock used in analog comparator override
// RESET: 0x00
// [2:0] cb_vref_mpll_reg_sel_rw__2__0__ - PLL reference voltage control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_105() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1adULL))) & vl::cat(vl::rd(cb_lpibias_en_ovr_en_rw), vl::rd(cb_bg_en_ovr_rw), vl::rd(cb_bg_en_ovr_en_rw), vl::rd(cb_atb_comp_clk_ovr_rw), vl::rd(cb_atb_comp_clk_ovr_en_rw), vl::rd(cb_vref_mpll_reg_sel_rw));
    testdouts_0[104].write(v_.bit(0).sl());
    testdouts_1[104].write(v_.bit(1).sl());
    testdouts_2[104].write(v_.bit(2).sl());
    testdouts_3[104].write(v_.bit(3).sl());
    testdouts_4[104].write(v_.bit(4).sl());
    testdouts_5[104].write(v_.bit(5).sl());
    testdouts_6[104].write(v_.bit(6).sl());
    testdouts_7[104].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CB_4 - Common block control
// OFFSET: 0x1AE
// ATTR: RW, CUST
// FIELDS:
// [7] cb_atb_on_ovr_en_rw - Analog Test Bus enable override
// RESET: 0x00
// [6:3] reserved_0 - Reserved field
// RESET: 0x00
// [2] cb_on_ovr_rw - Common Block power on override control
// RESET: 0x00
// [1] cb_on_ovr_en_rw - Common Block power on override control
// RESET: 0x00
// [0] cb_lpibias_en_ovr_rw - Override Enable voltage control to LPTX and currents for LPTX, LPRX
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_106() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aeULL))) & vl::cat(vl::rd(cb_atb_on_ovr_en_rw), vl::logic<4>(0x0ULL), vl::rd(cb_on_ovr_rw), vl::rd(cb_on_ovr_en_rw), vl::rd(cb_lpibias_en_ovr_rw));
    testdouts_0[105].write(v_.bit(0).sl());
    testdouts_1[105].write(v_.bit(1).sl());
    testdouts_2[105].write(v_.bit(2).sl());
    testdouts_3[105].write(v_.bit(3).sl());
    testdouts_4[105].write(v_.bit(4).sl());
    testdouts_5[105].write(v_.bit(5).sl());
    testdouts_6[105].write(v_.bit(6).sl());
    testdouts_7[105].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CB_5 - Common block control
// OFFSET: 0x1AF
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] cb_atb_on_ovr_rw - Analog Test Bus enable override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_107() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1afULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(cb_atb_on_ovr_rw));
    testdouts_0[106].write(v_.bit(0).sl());
    testdouts_1[106].write(v_.bit(1).sl());
    testdouts_2[106].write(v_.bit(2).sl());
    testdouts_3[106].write(v_.bit(3).sl());
    testdouts_4[106].write(v_.bit(4).sl());
    testdouts_5[106].write(v_.bit(5).sl());
    testdouts_6[106].write(v_.bit(6).sl());
    testdouts_7[106].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CB_6 - Common block control
// OFFSET: 0x1B0
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx_cb_lb_en_rext_if_rw - Static termination driver test, REXT enable
// RESET: 0x00
// [3:1] rx_cb_lb_setrx_if_rw__2__0__ -
// NOTES: comparator threshold (resistance mismatch to REXT)
// <br>: 3'b111: -20%
// <br>: 3'b011: -15%
// <br>: 3'b101: -10%
// <br>: 3'b001: -5%
// <br>: 3'b110: 5%
// <br>: 3'b010: 10%
// <br>: 3'b100: 15%
// <br>: 3'b000: 20%
// RESET: 0x00
// [0] rx_cb_ibias_en_if_rw - Enable currents for LP-TX, LP-RX and HS-RX
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_108() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1b0ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx_cb_lb_en_rext_if_rw), vl::rd(rx_cb_lb_setrx_if_rw), vl::rd(rx_cb_ibias_en_if_rw));
    testdouts_0[107].write(v_.bit(0).sl());
    testdouts_1[107].write(v_.bit(1).sl());
    testdouts_2[107].write(v_.bit(2).sl());
    testdouts_3[107].write(v_.bit(3).sl());
    testdouts_4[107].write(v_.bit(4).sl());
    testdouts_5[107].write(v_.bit(5).sl());
    testdouts_6[107].write(v_.bit(6).sl());
    testdouts_7[107].write(v_.bit(7).sl());
}

// %%CREG RD_RX_ANA_CB_0 - Common block observability
// OFFSET: 0x1C4
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] cb_on - Common block level shifter enable
// RESET: 0x00
// [4] cb_lpibias_en - Enable voltage currents for LP-TX and LP-RX
// RESET: 0x00
// [3] cb_ical_x4 - Programmable current for external resistor calibration
// RESET: 0x00
// [2] cb_bg_en - Bandgap enable
// RESET: 0x00
// [1] cb_atb_on - Analog test bus enable
// RESET: 0x00
// [0] cb_atb_comp_clk - Analog test bus comparator clock
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_109() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1c4ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(cb_on), vl::rd(cb_lpibias_en), vl::rd(cb_ical_x4), vl::rd(cb_bg_en), vl::rd(cb_atb_on), vl::rd(cb_atb_comp_clk));
    testdouts_0[108].write(v_.bit(0).sl());
    testdouts_1[108].write(v_.bit(1).sl());
    testdouts_2[108].write(v_.bit(2).sl());
    testdouts_3[108].write(v_.bit(3).sl());
    testdouts_4[108].write(v_.bit(4).sl());
    testdouts_5[108].write(v_.bit(5).sl());
    testdouts_6[108].write(v_.bit(6).sl());
    testdouts_7[108].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_DAC_0 - DAC/ADC control
// OFFSET: 0x1DA
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] adc_done_clear_rw - Clears last ADC procedure status
// RESET: 0x00
// [0] adc_en_rw - ADC enable, triggers ADC conversion
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_110() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1daULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(adc_done_clear_rw), vl::rd(adc_en_rw));
    testdouts_0[109].write(v_.bit(0).sl());
    testdouts_1[109].write(v_.bit(1).sl());
    testdouts_2[109].write(v_.bit(2).sl());
    testdouts_3[109].write(v_.bit(3).sl());
    testdouts_4[109].write(v_.bit(4).sl());
    testdouts_5[109].write(v_.bit(5).sl());
    testdouts_6[109].write(v_.bit(6).sl());
    testdouts_7[109].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_DAC_1 - DAC/ADC control
// OFFSET: 0x1DB
// ATTR: RW, CUST
// FIELDS:
// [7:0] cb_dac_prog_ovr_rw__7__0__ - Digital word to be translated to analog voltage override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_111() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1dbULL))) & vl::rd(cb_dac_prog_ovr_rw).slice<8>(0);
    testdouts_0[110].write(v_.bit(0).sl());
    testdouts_1[110].write(v_.bit(1).sl());
    testdouts_2[110].write(v_.bit(2).sl());
    testdouts_3[110].write(v_.bit(3).sl());
    testdouts_4[110].write(v_.bit(4).sl());
    testdouts_5[110].write(v_.bit(5).sl());
    testdouts_6[110].write(v_.bit(6).sl());
    testdouts_7[110].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_DAC_2 - DAC/ADC control
// OFFSET: 0x1DC
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] cb_dac_prog_ovr_rw__9__8__ - Digital word to be translated to analog voltage override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_112() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1dcULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(cb_dac_prog_ovr_rw).slice<2>(8));
    testdouts_0[111].write(v_.bit(0).sl());
    testdouts_1[111].write(v_.bit(1).sl());
    testdouts_2[111].write(v_.bit(2).sl());
    testdouts_3[111].write(v_.bit(3).sl());
    testdouts_4[111].write(v_.bit(4).sl());
    testdouts_5[111].write(v_.bit(5).sl());
    testdouts_6[111].write(v_.bit(6).sl());
    testdouts_7[111].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_DAC_3 - DAC/ADC control
// OFFSET: 0x1DD
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] cb_dac_prog_ovr_en_rw - Digital word to be translated to analog voltage override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_113() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1ddULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(cb_dac_prog_ovr_en_rw));
    testdouts_0[112].write(v_.bit(0).sl());
    testdouts_1[112].write(v_.bit(1).sl());
    testdouts_2[112].write(v_.bit(2).sl());
    testdouts_3[112].write(v_.bit(3).sl());
    testdouts_4[112].write(v_.bit(4).sl());
    testdouts_5[112].write(v_.bit(5).sl());
    testdouts_6[112].write(v_.bit(6).sl());
    testdouts_7[112].write(v_.bit(7).sl());
}

// %%CREG RD_RX_DAC_0 - DAC/ADC observability
// OFFSET: 0x1F2
// ATTR: READ, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] adc_done - Done flag for observation in ADC machine
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_114() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1f2ULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(adc_done));
    testdouts_0[113].write(v_.bit(0).sl());
    testdouts_1[113].write(v_.bit(1).sl());
    testdouts_2[113].write(v_.bit(2).sl());
    testdouts_3[113].write(v_.bit(3).sl());
    testdouts_4[113].write(v_.bit(4).sl());
    testdouts_5[113].write(v_.bit(5).sl());
    testdouts_6[113].write(v_.bit(6).sl());
    testdouts_7[113].write(v_.bit(7).sl());
}

// %%CREG RD_RX_DAC_1 - DAC/ADC observability
// OFFSET: 0x1F3
// ATTR: READ, CUST
// FIELDS:
// [7:0] cb_dac_prog__7__0__ - ADC digital word representing analog voltage SAR algorithm result
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_115() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1f3ULL))) & vl::rd(cb_dac_prog).slice<8>(0);
    testdouts_0[114].write(v_.bit(0).sl());
    testdouts_1[114].write(v_.bit(1).sl());
    testdouts_2[114].write(v_.bit(2).sl());
    testdouts_3[114].write(v_.bit(3).sl());
    testdouts_4[114].write(v_.bit(4).sl());
    testdouts_5[114].write(v_.bit(5).sl());
    testdouts_6[114].write(v_.bit(6).sl());
    testdouts_7[114].write(v_.bit(7).sl());
}

// %%CREG RD_RX_DAC_2 - DAC/ADC observability
// OFFSET: 0x1F4
// ATTR: READ, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] cb_dac_prog__9__8__ - ADC digital word representing analog voltage SAR algorithm result
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_116() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x1f4ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(cb_dac_prog).slice<2>(8));
    testdouts_0[115].write(v_.bit(0).sl());
    testdouts_1[115].write(v_.bit(1).sl());
    testdouts_2[115].write(v_.bit(2).sl());
    testdouts_3[115].write(v_.bit(3).sl());
    testdouts_4[115].write(v_.bit(4).sl());
    testdouts_5[115].write(v_.bit(5).sl());
    testdouts_6[115].write(v_.bit(6).sl());
    testdouts_7[115].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_TERM_CAL_0 - Termination calibration control
// OFFSET: 0x209
// ATTR: RW, CUST
// FIELDS:
// [7:4] cb_cal_repl_ovr_rw__3__0__ - Calibration word for termination lower section override
// RESET: 0x00
// [3:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] cb_cal_en_ovr_rw - Allow to override the lower part termination control
// RESET: 0x00
// [0] cb_cal_en_ovr_en_rw - Allow to override the lower part termination control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_117() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x209ULL))) & vl::cat(vl::rd(cb_cal_repl_ovr_rw), vl::logic<2>(0x0ULL), vl::rd(cb_cal_en_ovr_rw), vl::rd(cb_cal_en_ovr_en_rw));
    testdouts_0[116].write(v_.bit(0).sl());
    testdouts_1[116].write(v_.bit(1).sl());
    testdouts_2[116].write(v_.bit(2).sl());
    testdouts_3[116].write(v_.bit(3).sl());
    testdouts_4[116].write(v_.bit(4).sl());
    testdouts_5[116].write(v_.bit(5).sl());
    testdouts_6[116].write(v_.bit(6).sl());
    testdouts_7[116].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_TERM_CAL_1 - Termination calibration control
// OFFSET: 0x20A
// ATTR: RW, CUST
// FIELDS:
// [7] cb_en_45_ohm_rw - Selection for 45ohm termination impedance
// RESET: 0x00
// [6:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] cb_cal_repl_ovr_en_rw - Calibration word for termination lower section replica override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_118() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x20aULL))) & vl::cat(vl::rd(cb_en_45_ohm_rw), vl::logic<6>(0x0ULL), vl::rd(cb_cal_repl_ovr_en_rw));
    testdouts_0[117].write(v_.bit(0).sl());
    testdouts_1[117].write(v_.bit(1).sl());
    testdouts_2[117].write(v_.bit(2).sl());
    testdouts_3[117].write(v_.bit(3).sl());
    testdouts_4[117].write(v_.bit(4).sl());
    testdouts_5[117].write(v_.bit(5).sl());
    testdouts_6[117].write(v_.bit(6).sl());
    testdouts_7[117].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_TERM_CAL_2 - Termination calibration control
// OFFSET: 0x20B
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] cb_ical_x4_ovr_rw - Programming current for termination calibration algorithm override
// RESET: 0x00
// [0] cb_ical_x4_ovr_en_rw - Programming current for termination calibration algorithm override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_119() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x20bULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(cb_ical_x4_ovr_rw), vl::rd(cb_ical_x4_ovr_en_rw));
    testdouts_0[118].write(v_.bit(0).sl());
    testdouts_1[118].write(v_.bit(1).sl());
    testdouts_2[118].write(v_.bit(2).sl());
    testdouts_3[118].write(v_.bit(3).sl());
    testdouts_4[118].write(v_.bit(4).sl());
    testdouts_5[118].write(v_.bit(5).sl());
    testdouts_6[118].write(v_.bit(6).sl());
    testdouts_7[118].write(v_.bit(7).sl());
}

// %%CREG RD_RX_TERM_CAL_0 - Termination calibration observability
// OFFSET: 0x220
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5:2] cb_cal_repl__3__0__ - Termination lower section replica setting
// RESET: 0x00
// [1] reserved_1 - Reserved field
// RESET: 0x00
// [0] cb_cal_en - Termination lower section analog circuitry enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_120() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x220ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(cb_cal_repl), vl::logic<1>(0x0ULL), vl::rd(cb_cal_en));
    testdouts_0[119].write(v_.bit(0).sl());
    testdouts_1[119].write(v_.bit(1).sl());
    testdouts_2[119].write(v_.bit(2).sl());
    testdouts_3[119].write(v_.bit(3).sl());
    testdouts_4[119].write(v_.bit(4).sl());
    testdouts_5[119].write(v_.bit(5).sl());
    testdouts_6[119].write(v_.bit(6).sl());
    testdouts_7[119].write(v_.bit(7).sl());
}

// %%CREG RD_RX_TERM_CAL_1 - Termination calibration observability
// OFFSET: 0x221
// ATTR: READ, CUST
// FIELDS:
// [7] rescal_done - Lower section termination calibration algorithm done
// RESET: 0x00
// [6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rescal_en - Lower section termination calibration algorithm enable
// RESET: 0x00
// [4] cb_comp_out - Analog comparator output
// RESET: 0x00
// [3:0] reserved_1 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_121() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x221ULL))) & vl::cat(vl::rd(rescal_done), vl::logic<1>(0x0ULL), vl::rd(rescal_en), vl::rd(cb_comp_out), vl::logic<4>(0x0ULL));
    testdouts_0[120].write(v_.bit(0).sl());
    testdouts_1[120].write(v_.bit(1).sl());
    testdouts_2[120].write(v_.bit(2).sl());
    testdouts_3[120].write(v_.bit(3).sl());
    testdouts_4[120].write(v_.bit(4).sl());
    testdouts_5[120].write(v_.bit(5).sl());
    testdouts_6[120].write(v_.bit(6).sl());
    testdouts_7[120].write(v_.bit(7).sl());
}

// %%CREG RD_RX_TERM_CAL_2 - Termination calibration observability
// OFFSET: 0x222
// ATTR: READ, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] rescal_error - Termination lower section calibration algorithm error
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_122() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x222ULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(rescal_error));
    testdouts_0[121].write(v_.bit(0).sl());
    testdouts_1[121].write(v_.bit(1).sl());
    testdouts_2[121].write(v_.bit(2).sl());
    testdouts_3[121].write(v_.bit(3).sl());
    testdouts_4[121].write(v_.bit(4).sl());
    testdouts_5[121].write(v_.bit(5).sl());
    testdouts_6[121].write(v_.bit(6).sl());
    testdouts_7[121].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_NOREXTTUNE_0 - Termination calibration without external resistor control
// OFFSET: 0x237
// ATTR: RW, CUST
// FIELDS:
// [7:0] count_ref_rw__7__0__ - Termination calibration without REXT decision threshold (to increase/decrease resistance)
// RESET: 0x96
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_123() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x237ULL))) & vl::rd(count_ref_rw).slice<8>(0);
    testdouts_0[122].write(v_.bit(0).sl());
    testdouts_1[122].write(v_.bit(1).sl());
    testdouts_2[122].write(v_.bit(2).sl());
    testdouts_3[122].write(v_.bit(3).sl());
    testdouts_4[122].write(v_.bit(4).sl());
    testdouts_5[122].write(v_.bit(5).sl());
    testdouts_6[122].write(v_.bit(6).sl());
    testdouts_7[122].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_NOREXTTUNE_1 - Termination calibration without external resistor control
// OFFSET: 0x238
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] count_ref_rw__9__8__ - Termination calibration without REXT decision threshold (to increase/decrease resistance)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_124() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x238ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(count_ref_rw).slice<2>(8));
    testdouts_0[123].write(v_.bit(0).sl());
    testdouts_1[123].write(v_.bit(1).sl());
    testdouts_2[123].write(v_.bit(2).sl());
    testdouts_3[123].write(v_.bit(3).sl());
    testdouts_4[123].write(v_.bit(4).sl());
    testdouts_5[123].write(v_.bit(5).sl());
    testdouts_6[123].write(v_.bit(6).sl());
    testdouts_7[123].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_NOREXTTUNE_2 - Termination calibration without external resistor control
// OFFSET: 0x239
// ATTR: RW, CUST
// FIELDS:
// [7:4] setr_min_rw__3__0__ - Termination calibration without REXT minimum oscillation threshold
// RESET: 0x04
// [3:0] setr_max_rw__3__0__ - Termination calibration without REXT maximum oscillation threshold
// RESET: 0x0B
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_125() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x239ULL))) & vl::cat(vl::rd(setr_min_rw), vl::rd(setr_max_rw));
    testdouts_0[124].write(v_.bit(0).sl());
    testdouts_1[124].write(v_.bit(1).sl());
    testdouts_2[124].write(v_.bit(2).sl());
    testdouts_3[124].write(v_.bit(3).sl());
    testdouts_4[124].write(v_.bit(4).sl());
    testdouts_5[124].write(v_.bit(5).sl());
    testdouts_6[124].write(v_.bit(6).sl());
    testdouts_7[124].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_NOREXTTUNE_3 - Termination calibration without external resistor control
// OFFSET: 0x23A
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] setr_typ_rw__3__0__ - Termination calibration without REXT typical oscillation threshold
// RESET: 0x07
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_126() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x23aULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(setr_typ_rw));
    testdouts_0[125].write(v_.bit(0).sl());
    testdouts_1[125].write(v_.bit(1).sl());
    testdouts_2[125].write(v_.bit(2).sl());
    testdouts_3[125].write(v_.bit(3).sl());
    testdouts_4[125].write(v_.bit(4).sl());
    testdouts_5[125].write(v_.bit(5).sl());
    testdouts_6[125].write(v_.bit(6).sl());
    testdouts_7[125].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_NOREXTTUNE_4 - Termination calibration without external resistor control
// OFFSET: 0x23B
// ATTR: RW, CUST
// FIELDS:
// [7:0] stability_time_rw__7__0__ - Additional stability time for internal oscillator used for termination calibration without REXT
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_127() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x23bULL))) & vl::rd(stability_time_rw);
    testdouts_0[126].write(v_.bit(0).sl());
    testdouts_1[126].write(v_.bit(1).sl());
    testdouts_2[126].write(v_.bit(2).sl());
    testdouts_3[126].write(v_.bit(3).sl());
    testdouts_4[126].write(v_.bit(4).sl());
    testdouts_5[126].write(v_.bit(5).sl());
    testdouts_6[126].write(v_.bit(6).sl());
    testdouts_7[126].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_NOREXTTUNE_5 - Termination calibration without external resistor control
// OFFSET: 0x23C
// ATTR: RW, CUST
// FIELDS:
// [7:5] rx_rxhs_setr_ovr_rw__2__0__ - Termination calibration word override (needs rx_rxhs_setr_ovr_en = 1'b1)
// RESET: 0x00
// [4] rx_rxhs_setr_ovr_en_rw - Termination calibration word override enable
// RESET: 0x00
// [3] rxclk_rxhs_osc_res_en_ovr_rw - Oscillator mode (poly/MOS) to analog circuitry override
// RESET: 0x00
// [2] rxclk_rxhs_osc_res_en_ovr_en_rw - Oscillator mode (poly/MOS) to analog circuitry override enable
// RESET: 0x00
// [1] rxclk_rxhs_osc_pon_ovr_rw - Oscillator power-on override
// RESET: 0x00
// [0] rxclk_rxhs_osc_pon_ovr_en_rw - Oscillator power-on override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_128() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x23cULL))) & vl::cat(vl::rd(rx_rxhs_setr_ovr_rw), vl::rd(rx_rxhs_setr_ovr_en_rw), vl::rd(rxclk_rxhs_osc_res_en_ovr_rw), vl::rd(rxclk_rxhs_osc_res_en_ovr_en_rw), vl::rd(rxclk_rxhs_osc_pon_ovr_rw), vl::rd(rxclk_rxhs_osc_pon_ovr_en_rw));
    testdouts_0[127].write(v_.bit(0).sl());
    testdouts_1[127].write(v_.bit(1).sl());
    testdouts_2[127].write(v_.bit(2).sl());
    testdouts_3[127].write(v_.bit(3).sl());
    testdouts_4[127].write(v_.bit(4).sl());
    testdouts_5[127].write(v_.bit(5).sl());
    testdouts_6[127].write(v_.bit(6).sl());
    testdouts_7[127].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_NOREXTTUNE_6 - Termination calibration without external resistor control
// OFFSET: 0x23D
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] burnin_cal_en_ovr_rw - Burnin oscillator setting override value (needs burnin_cal_en_ovr_en = 1'b1)
// RESET: 0x00
// [2] burnin_cal_en_ovr_en_rw - Burnin oscillator setting override value (needs burnin_cal_en_ovr_en = 1'b1)
// RESET: 0x00
// [1] norexttune_done_ovr_rw - Termination calibration (without external reference resistor) override control
// RESET: 0x00
// [0] norexttune_done_ovr_en_rw - Termination calibration (without external reference resistor) override control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_129() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x23dULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(burnin_cal_en_ovr_rw), vl::rd(burnin_cal_en_ovr_en_rw), vl::rd(norexttune_done_ovr_rw), vl::rd(norexttune_done_ovr_en_rw));
    testdouts_0[128].write(v_.bit(0).sl());
    testdouts_1[128].write(v_.bit(1).sl());
    testdouts_2[128].write(v_.bit(2).sl());
    testdouts_3[128].write(v_.bit(3).sl());
    testdouts_4[128].write(v_.bit(4).sl());
    testdouts_5[128].write(v_.bit(5).sl());
    testdouts_6[128].write(v_.bit(6).sl());
    testdouts_7[128].write(v_.bit(7).sl());
}

// %%CREG RD_RX_NOREXTTUNE_0 - Termination calibration without external resistor observability
// OFFSET: 0x252
// ATTR: READ, CUST
// FIELDS:
// [7:0] count_osc0__7__0__ - Counter Oscillator
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_130() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x252ULL))) & vl::rd(count_osc0).slice<8>(0);
    testdouts_0[129].write(v_.bit(0).sl());
    testdouts_1[129].write(v_.bit(1).sl());
    testdouts_2[129].write(v_.bit(2).sl());
    testdouts_3[129].write(v_.bit(3).sl());
    testdouts_4[129].write(v_.bit(4).sl());
    testdouts_5[129].write(v_.bit(5).sl());
    testdouts_6[129].write(v_.bit(6).sl());
    testdouts_7[129].write(v_.bit(7).sl());
}

// %%CREG RD_RX_NOREXTTUNE_1 - Termination calibration without external resistor observability
// OFFSET: 0x253
// ATTR: READ, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] count_osc0__9__8__ - Counter Oscillator
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_131() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x253ULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(count_osc0).slice<2>(8));
    testdouts_0[130].write(v_.bit(0).sl());
    testdouts_1[130].write(v_.bit(1).sl());
    testdouts_2[130].write(v_.bit(2).sl());
    testdouts_3[130].write(v_.bit(3).sl());
    testdouts_4[130].write(v_.bit(4).sl());
    testdouts_5[130].write(v_.bit(5).sl());
    testdouts_6[130].write(v_.bit(6).sl());
    testdouts_7[130].write(v_.bit(7).sl());
}

// %%CREG RD_RX_NOREXTTUNE_2 - Termination calibration without external resistor observability
// OFFSET: 0x254
// ATTR: READ, CUST
// FIELDS:
// [7:0] count_osc1__7__0__ - Counter oscillator
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_132() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x254ULL))) & vl::rd(count_osc1).slice<8>(0);
    testdouts_0[131].write(v_.bit(0).sl());
    testdouts_1[131].write(v_.bit(1).sl());
    testdouts_2[131].write(v_.bit(2).sl());
    testdouts_3[131].write(v_.bit(3).sl());
    testdouts_4[131].write(v_.bit(4).sl());
    testdouts_5[131].write(v_.bit(5).sl());
    testdouts_6[131].write(v_.bit(6).sl());
    testdouts_7[131].write(v_.bit(7).sl());
}

// %%CREG RD_RX_NOREXTTUNE_3 - Termination calibration without external resistor observability
// OFFSET: 0x255
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5:2] setr_burnin__3__0__ - NOREXT calibration result word
// RESET: 0x00
// [1:0] count_osc1__9__8__ - Counter oscillator
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_133() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x255ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(setr_burnin), vl::rd(count_osc1).slice<2>(8));
    testdouts_0[132].write(v_.bit(0).sl());
    testdouts_1[132].write(v_.bit(1).sl());
    testdouts_2[132].write(v_.bit(2).sl());
    testdouts_3[132].write(v_.bit(3).sl());
    testdouts_4[132].write(v_.bit(4).sl());
    testdouts_5[132].write(v_.bit(5).sl());
    testdouts_6[132].write(v_.bit(6).sl());
    testdouts_7[132].write(v_.bit(7).sl());
}

// %%CREG RD_RX_NOREXTTUNE_4 - Termination calibration without external resistor observability
// OFFSET: 0x256
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] noext_burn_error - Termination calibration without external resistor error
// RESET: 0x00
// [5] norexttune_done - Termination calibration without external resistor done
// RESET: 0x00
// [4:2] rx_rxhs_setr__2__0__ - Termination setting
// RESET: 0x00
// [1] rxclk_rxhs_osc_res_en - Burn-in oscilator mode enable
// RESET: 0x00
// [0] rxclk_rxhs_osc_pon - Burn-in oscilator power on
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_134() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x256ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(noext_burn_error), vl::rd(norexttune_done), vl::rd(rx_rxhs_setr), vl::rd(rxclk_rxhs_osc_res_en), vl::rd(rxclk_rxhs_osc_pon));
    testdouts_0[133].write(v_.bit(0).sl());
    testdouts_1[133].write(v_.bit(1).sl());
    testdouts_2[133].write(v_.bit(2).sl());
    testdouts_3[133].write(v_.bit(3).sl());
    testdouts_4[133].write(v_.bit(4).sl());
    testdouts_5[133].write(v_.bit(5).sl());
    testdouts_6[133].write(v_.bit(6).sl());
    testdouts_7[133].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_LANE_0 - Clock lane control
// OFFSET: 0x301
// ATTR: RW, CUST
// FIELDS:
// [7] hsrx_en_bypass_clklane_rw - Clock lane HS receiver override enable
// RESET: 0x00
// [6] bintpon_clklane_rw - Bias block power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [5] bintpon_bypass_clklane_rw - Bias block power-on bypass override
// RESET: 0x00
// [4:1] atb_sel_clklane_rw__3__0__ -
// NOTES: Analog Test Bus signals selection:
// <br> 000: disconnected
// <br> 001: vp
// <br> 010: vpclk
// <br> 011: gd
// <br> 100: Voltage steering termination switches
// RESET: 0x00
// [0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_135() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x301ULL))) & vl::cat(vl::rd(hsrx_en_bypass_clklane_rw), vl::rd(bintpon_clklane_rw), vl::rd(bintpon_bypass_clklane_rw), vl::rd(atb_sel_clklane_rw), vl::logic<1>(0x0ULL));
    testdouts_0[134].write(v_.bit(0).sl());
    testdouts_1[134].write(v_.bit(1).sl());
    testdouts_2[134].write(v_.bit(2).sl());
    testdouts_3[134].write(v_.bit(3).sl());
    testdouts_4[134].write(v_.bit(4).sl());
    testdouts_5[134].write(v_.bit(5).sl());
    testdouts_6[134].write(v_.bit(6).sl());
    testdouts_7[134].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_LANE_1 - Clock lane control
// OFFSET: 0x302
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] hsrxpon_if_clklane_rw - Power on of RX HS
// RESET: 0x00
// [3] hsrxpon_bypass_clklane_rw - Power on of RX HS bypass bit
// RESET: 0x00
// [2] hsrxenterm_if_clklane_rw - RX HS enable term bit
// RESET: 0x00
// [1] hsrxenterm_bypass_clklane_rw - RX HS enable term bypass bit
// RESET: 0x00
// [0] hsrx_en_if_clklane_tester_rw - HS RX clock lane enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_136() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x302ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(hsrxpon_if_clklane_rw), vl::rd(hsrxpon_bypass_clklane_rw), vl::rd(hsrxenterm_if_clklane_rw), vl::rd(hsrxenterm_bypass_clklane_rw), vl::rd(hsrx_en_if_clklane_tester_rw));
    testdouts_0[135].write(v_.bit(0).sl());
    testdouts_1[135].write(v_.bit(1).sl());
    testdouts_2[135].write(v_.bit(2).sl());
    testdouts_3[135].write(v_.bit(3).sl());
    testdouts_4[135].write(v_.bit(4).sl());
    testdouts_5[135].write(v_.bit(5).sl());
    testdouts_6[135].write(v_.bit(6).sl());
    testdouts_7[135].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_LANE_3 - Clock lane control
// OFFSET: 0x304
// ATTR: RW, CUST
// FIELDS:
// [7] lprxponcd_bypass_clklane_rw - Power on the low power contention detector receiver bypass control
// RESET: 0x00
// [6:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_137() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x304ULL))) & vl::cat(vl::rd(lprxponcd_bypass_clklane_rw), vl::logic<7>(0x0ULL));
    testdouts_0[136].write(v_.bit(0).sl());
    testdouts_1[136].write(v_.bit(1).sl());
    testdouts_2[136].write(v_.bit(2).sl());
    testdouts_3[136].write(v_.bit(3).sl());
    testdouts_4[136].write(v_.bit(4).sl());
    testdouts_5[136].write(v_.bit(5).sl());
    testdouts_6[136].write(v_.bit(6).sl());
    testdouts_7[136].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_LANE_4 - Clock lane control
// OFFSET: 0x305
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] lprxponulp_clklane_rw - Power on of ULP mode
// RESET: 0x00
// [3] lprxponulp_bypass_clklane_rw - Power on of ULP mode of bypass control
// RESET: 0x00
// [2] lprxponlp_clklane_rw - Power on of the low power receiver
// RESET: 0x00
// [1] lprxponlp_bypass_clklane_rw - Power on of the low power receiver bypass control
// RESET: 0x00
// [0] lprxponcd_clklane_rw - Power on the low power contention detector receiver
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_138() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x305ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(lprxponulp_clklane_rw), vl::rd(lprxponulp_bypass_clklane_rw), vl::rd(lprxponlp_clklane_rw), vl::rd(lprxponlp_bypass_clklane_rw), vl::rd(lprxponcd_clklane_rw));
    testdouts_0[137].write(v_.bit(0).sl());
    testdouts_1[137].write(v_.bit(1).sl());
    testdouts_2[137].write(v_.bit(2).sl());
    testdouts_3[137].write(v_.bit(3).sl());
    testdouts_4[137].write(v_.bit(4).sl());
    testdouts_5[137].write(v_.bit(5).sl());
    testdouts_6[137].write(v_.bit(6).sl());
    testdouts_7[137].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_LANE_6 - Clock lane control
// OFFSET: 0x307
// ATTR: RW, CUST
// FIELDS:
// [7] rxclk_rxhs_pull_long_channel_if_rw - RX HS pull long channel
// RESET: 0x00
// [6] rxclk_rxhs_int_clk_sel_rw - RX HS internal clock selection
// RESET: 0x00
// [5] rxclk_rxhs_feed_int_clk_ovr_en_rw - RX HS source data override enable control
// RESET: 0x00
// [4] rxclk_rxhs_feed_int_clk_ovr_rw - RX HS source data override
// RESET: 0x00
// [3] rxclk_rxhs_ddr_clk_en_if_rw - RX HS DDR clock enable bit
// RESET: 0x00
// [2] rxclk_rxhs_clk_to_long_channel_if_rw - RX HS clock to long channel bits
// RESET: 0x00
// [1] reserved_0 - Reserved field
// RESET: 0x00
// [0] polarity_clklane_rw - Clock lane dp/dn swap. Active high
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_139() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x307ULL))) & vl::cat(vl::rd(rxclk_rxhs_pull_long_channel_if_rw), vl::rd(rxclk_rxhs_int_clk_sel_rw), vl::rd(rxclk_rxhs_feed_int_clk_ovr_en_rw), vl::rd(rxclk_rxhs_feed_int_clk_ovr_rw), vl::rd(rxclk_rxhs_ddr_clk_en_if_rw), vl::rd(rxclk_rxhs_clk_to_long_channel_if_rw), vl::logic<1>(0x0ULL), vl::rd(polarity_clklane_rw));
    testdouts_0[138].write(v_.bit(0).sl());
    testdouts_1[138].write(v_.bit(1).sl());
    testdouts_2[138].write(v_.bit(2).sl());
    testdouts_3[138].write(v_.bit(3).sl());
    testdouts_4[138].write(v_.bit(4).sl());
    testdouts_5[138].write(v_.bit(5).sl());
    testdouts_6[138].write(v_.bit(6).sl());
    testdouts_7[138].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_LANE_7 - Clock lane control
// OFFSET: 0x308
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rxclk_rxhs_rx_en200mode_if_rw - Clock lane's 200 ohm differential termination enable control
// RESET: 0x00
// [3] rxclk_rxhs_ddr_clk_sel_if_rw - DDR clock lane source selection control (internal or external through long channel)
// RESET: 0x00
// [2:1] rxclk_rxhs_ddr_clk_div_ovr_rw__1__0__ - Clock lane's DDR clock division factor override
// RESET: 0x00
// [0] rxclk_rxhs_ddr_clk_div_ovr_en_rw - Clock lane's DDR clock division factor override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_140() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x308ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rxclk_rxhs_rx_en200mode_if_rw), vl::rd(rxclk_rxhs_ddr_clk_sel_if_rw), vl::rd(rxclk_rxhs_ddr_clk_div_ovr_rw), vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en_rw));
    testdouts_0[139].write(v_.bit(0).sl());
    testdouts_1[139].write(v_.bit(1).sl());
    testdouts_2[139].write(v_.bit(2).sl());
    testdouts_3[139].write(v_.bit(3).sl());
    testdouts_4[139].write(v_.bit(4).sl());
    testdouts_5[139].write(v_.bit(5).sl());
    testdouts_6[139].write(v_.bit(6).sl());
    testdouts_7[139].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_LANE_8 - Clock lane termination test control
// OFFSET: 0x309
// ATTR: RW, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:3] rxclk_lb_prog_if_rw__3__0__ - clock lane receiver biasing currents
// RESET: 0x0F
// [2] rxclk_lb_res_cm_en_if_rw - clock lane static termination driver middle point pull down enable
// RESET: 0x00
// [1] rxclk_lb_res_dn_en_if_rw - clock lane static termination driver test dn line enable
// RESET: 0x00
// [0] rxclk_lb_res_dp_en_if_rw - clock lane static termination driver test dp line enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_141() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x309ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rxclk_lb_prog_if_rw), vl::rd(rxclk_lb_res_cm_en_if_rw), vl::rd(rxclk_lb_res_dn_en_if_rw), vl::rd(rxclk_lb_res_dp_en_if_rw));
    testdouts_0[140].write(v_.bit(0).sl());
    testdouts_1[140].write(v_.bit(1).sl());
    testdouts_2[140].write(v_.bit(2).sl());
    testdouts_3[140].write(v_.bit(3).sl());
    testdouts_4[140].write(v_.bit(4).sl());
    testdouts_5[140].write(v_.bit(5).sl());
    testdouts_6[140].write(v_.bit(6).sl());
    testdouts_7[140].write(v_.bit(7).sl());
}

// %%CREG RD_RX_CLKLANE_LANE_0 - Clock lane observability
// OFFSET: 0x32B
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:5] lprxdoutlp_clklane_if__1__0__ - LP-RX receiver outputs
// RESET: 0x00
// [4:3] lprxdoutcd_clklane_if__1__0__ - LP-CD receiver outputs
// RESET: 0x00
// [2] hsrx_en_if_clklane - HS RX clock lane enable
// RESET: 0x00
// [1] enable_clklane - Enable signal for clock lane
// RESET: 0x00
// [0] bintpon_if_clklane - Bias block power-on override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_142() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x32bULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(lprxdoutlp_clklane_if), vl::rd(lprxdoutcd_clklane_if), vl::rd(hsrx_en_if_clklane), vl::rd(enable_clklane), vl::rd(bintpon_if_clklane));
    testdouts_0[141].write(v_.bit(0).sl());
    testdouts_1[141].write(v_.bit(1).sl());
    testdouts_2[141].write(v_.bit(2).sl());
    testdouts_3[141].write(v_.bit(3).sl());
    testdouts_4[141].write(v_.bit(4).sl());
    testdouts_5[141].write(v_.bit(5).sl());
    testdouts_6[141].write(v_.bit(6).sl());
    testdouts_7[141].write(v_.bit(7).sl());
}

// %%CREG RD_RX_CLKLANE_LANE_1 - Clock lane observability
// OFFSET: 0x32C
// ATTR: READ, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1:0] lprxdoutulp_clklane_if__1__0__ - Low power RX ULP output data
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_143() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x32cULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(lprxdoutulp_clklane_if));
    testdouts_0[142].write(v_.bit(0).sl());
    testdouts_1[142].write(v_.bit(1).sl());
    testdouts_2[142].write(v_.bit(2).sl());
    testdouts_3[142].write(v_.bit(3).sl());
    testdouts_4[142].write(v_.bit(4).sl());
    testdouts_5[142].write(v_.bit(5).sl());
    testdouts_6[142].write(v_.bit(6).sl());
    testdouts_7[142].write(v_.bit(7).sl());
}

// %%CREG RD_RX_CLKLANE_LANE_2 - Clock lane observability
// OFFSET: 0x32D
// ATTR: READ, CUST
// FIELDS:
// [7] rxclk_rxhs_rx_enterm_if - Clock lane HS termination enable
// RESET: 0x00
// [6] rxclk_rxhs_rx_enlogic_if - Clock lane level shifter enable
// RESET: 0x00
// [5] rxclk_rxhs_rx_en_if - Clock lane HS receiver enable
// RESET: 0x00
// [4] rxclk_rxhs_osc_out_if - Internal burnin oscillator clock
// RESET: 0x00
// [3] rxclk_rxhs_ddr_clk_if - Clock lane's divided DDR clock
// RESET: 0x00
// [2:1] rxclk_rxhs_ddr_clk_div__1__0__ - DDR clock division ratio
// RESET: 0x00
// [0] lsrxclk_clklane - Low power clock lane clock
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_144() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x32dULL))) & vl::cat(vl::rd(rxclk_rxhs_rx_enterm_if), vl::rd(rxclk_rxhs_rx_enlogic_if), vl::rd(rxclk_rxhs_rx_en_if), vl::rd(rxclk_rxhs_osc_out_if), vl::rd(rxclk_rxhs_ddr_clk_if), vl::rd(rxclk_rxhs_ddr_clk_div), vl::rd(lsrxclk_clklane));
    testdouts_0[143].write(v_.bit(0).sl());
    testdouts_1[143].write(v_.bit(1).sl());
    testdouts_2[143].write(v_.bit(2).sl());
    testdouts_3[143].write(v_.bit(3).sl());
    testdouts_4[143].write(v_.bit(4).sl());
    testdouts_5[143].write(v_.bit(5).sl());
    testdouts_6[143].write(v_.bit(6).sl());
    testdouts_7[143].write(v_.bit(7).sl());
}

// %%CREG RD_RX_CLKLANE_LANE_3 - Clock lane observability
// OFFSET: 0x32E
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rxclk_rxlp_ponulp_if - Clock lane ULP receiver
// RESET: 0x00
// [2] rxclk_rxlp_ponlp_if - Clock lane LP receiver
// RESET: 0x00
// [1] rxclk_rxlp_poncd_if - Clock lane LP contention detector
// RESET: 0x00
// [0] rxclk_rxhs_rx_pon_if - Clock lane HS receiver power on
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_145() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x32eULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rxclk_rxlp_ponulp_if), vl::rd(rxclk_rxlp_ponlp_if), vl::rd(rxclk_rxlp_poncd_if), vl::rd(rxclk_rxhs_rx_pon_if));
    testdouts_0[144].write(v_.bit(0).sl());
    testdouts_1[144].write(v_.bit(1).sl());
    testdouts_2[144].write(v_.bit(2).sl());
    testdouts_3[144].write(v_.bit(3).sl());
    testdouts_4[144].write(v_.bit(4).sl());
    testdouts_5[144].write(v_.bit(5).sl());
    testdouts_6[144].write(v_.bit(6).sl());
    testdouts_7[144].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_OFFSET_CAL_0 - Clock lane offset cancellation control
// OFFSET: 0x37A
// ATTR: RW, CUST
// FIELDS:
// [7:2] rxclk_rxhs_clk_offset_cal_ovr_rw__5__0__ - RX HS clock offset calibration override control
// RESET: 0x00
// [1] rxclk_caldone_ovr_rw - Clock lane offset calibration done override
// RESET: 0x00
// [0] rxclk_caldone_ovr_en_rw - Clock lane offset calibration done override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_146() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x37aULL))) & vl::cat(vl::rd(rxclk_rxhs_clk_offset_cal_ovr_rw), vl::rd(rxclk_caldone_ovr_rw), vl::rd(rxclk_caldone_ovr_en_rw));
    testdouts_0[145].write(v_.bit(0).sl());
    testdouts_1[145].write(v_.bit(1).sl());
    testdouts_2[145].write(v_.bit(2).sl());
    testdouts_3[145].write(v_.bit(3).sl());
    testdouts_4[145].write(v_.bit(4).sl());
    testdouts_5[145].write(v_.bit(5).sl());
    testdouts_6[145].write(v_.bit(6).sl());
    testdouts_7[145].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_CLKLANE_OFFSET_CAL_1 - Clock lane offset cancellation control
// OFFSET: 0x37B
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rxclk_start_calibration_ovr_rw - Clock lane digital offset start calibration override enable control
// RESET: 0x00
// [3] rxclk_start_calibration_ovr_en_rw - Clock lane digital offset start calibration override enable control
// RESET: 0x00
// [2] rxclk_rxhs_start_calibration_ovr_rw - Clock lane analog offset start calibration override enable control
// RESET: 0x00
// [1] rxclk_rxhs_start_calibration_ovr_en_rw - Clock lane analog offset start calibration override enable control
// RESET: 0x00
// [0] rxclk_rxhs_clk_offset_cal_ovr_en_rw - RX HS clock offset calibration override enable control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_147() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x37bULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rxclk_start_calibration_ovr_rw), vl::rd(rxclk_start_calibration_ovr_en_rw), vl::rd(rxclk_rxhs_start_calibration_ovr_rw), vl::rd(rxclk_rxhs_start_calibration_ovr_en_rw), vl::rd(rxclk_rxhs_clk_offset_cal_ovr_en_rw));
    testdouts_0[146].write(v_.bit(0).sl());
    testdouts_1[146].write(v_.bit(1).sl());
    testdouts_2[146].write(v_.bit(2).sl());
    testdouts_3[146].write(v_.bit(3).sl());
    testdouts_4[146].write(v_.bit(4).sl());
    testdouts_5[146].write(v_.bit(5).sl());
    testdouts_6[146].write(v_.bit(6).sl());
    testdouts_7[146].write(v_.bit(7).sl());
}

// %%CREG RD_RX_CLKLANE_OFFSET_CAL_0 - Clock lane offset cancellation observability
// OFFSET: 0x39D
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rxclk_rxhs_clk_m_cal - Clock lane offset calibration m-line comparator output
// RESET: 0x00
// [4:1] rxclk_errcal__3__0__ - Clock lane offset calibration error
// RESET: 0x00
// [0] rxclk_caldone - Clock lane offset calibration done
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_148() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x39dULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rxclk_rxhs_clk_m_cal), vl::rd(rxclk_errcal), vl::rd(rxclk_caldone));
    testdouts_0[147].write(v_.bit(0).sl());
    testdouts_1[147].write(v_.bit(1).sl());
    testdouts_2[147].write(v_.bit(2).sl());
    testdouts_3[147].write(v_.bit(3).sl());
    testdouts_4[147].write(v_.bit(4).sl());
    testdouts_5[147].write(v_.bit(5).sl());
    testdouts_6[147].write(v_.bit(6).sl());
    testdouts_7[147].write(v_.bit(7).sl());
}

// %%CREG RD_RX_CLKLANE_OFFSET_CAL_1 - Clock lane offset cancellation observability
// OFFSET: 0x39E
// ATTR: READ, CUST
// FIELDS:
// [7] rxclk_rxhs_start_calibration - Clock lane analog offset start calibration
// RESET: 0x00
// [6] rxclk_rxhs_clk_p_cal - Clock lane offset calibration p-line comparator output
// RESET: 0x00
// [5:0] rxclk_rxhs_clk_offset_cal__5__0__ - Clock lane offset calibration setting
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_149() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x39eULL))) & vl::cat(vl::rd(rxclk_rxhs_start_calibration), vl::rd(rxclk_rxhs_clk_p_cal), vl::rd(rxclk_rxhs_clk_offset_cal));
    testdouts_0[148].write(v_.bit(0).sl());
    testdouts_1[148].write(v_.bit(1).sl());
    testdouts_2[148].write(v_.bit(2).sl());
    testdouts_3[148].write(v_.bit(3).sl());
    testdouts_4[148].write(v_.bit(4).sl());
    testdouts_5[148].write(v_.bit(5).sl());
    testdouts_6[148].write(v_.bit(6).sl());
    testdouts_7[148].write(v_.bit(7).sl());
}

// %%CREG RD_RX_CLKLANE_OFFSET_CAL_2 - Clock lane offset cancellation observability
// OFFSET: 0x39F
// ATTR: READ, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] rxclk_start_calibration - Clock lane digital offset start calibration
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_150() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x39fULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(rxclk_start_calibration));
    testdouts_0[149].write(v_.bit(0).sl());
    testdouts_1[149].write(v_.bit(1).sl());
    testdouts_2[149].write(v_.bit(2).sl());
    testdouts_3[149].write(v_.bit(3).sl());
    testdouts_4[149].write(v_.bit(4).sl());
    testdouts_5[149].write(v_.bit(5).sl());
    testdouts_6[149].write(v_.bit(6).sl());
    testdouts_7[149].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_0 - Lane 0 control
// OFFSET: 0x501
// ATTR: RW, CUST
// FIELDS:
// [7] hsrx_en_bypass_lane0_rw - HS RX enable bypass
// RESET: 0x00
// [6] bintpon_lane0_rw - Bias block power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [5] bintpon_bypass_lane0_rw - Bias block power-on bypass override
// RESET: 0x00
// [4:1] atb_sel_lane0_rw__3__0__ - Analog Test Bus signals selection:
// RESET: 0x00
// [0] atb_lptx1200_on_lane0_rw - Selection to to probe in analog test bus LP transmitter voltage reference
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_151() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x501ULL))) & vl::cat(vl::rd(hsrx_en_bypass_lane0_rw), vl::rd(bintpon_lane0_rw), vl::rd(bintpon_bypass_lane0_rw), vl::rd(atb_sel_lane0_rw), vl::rd(atb_lptx1200_on_lane0_rw));
    testdouts_0[150].write(v_.bit(0).sl());
    testdouts_1[150].write(v_.bit(1).sl());
    testdouts_2[150].write(v_.bit(2).sl());
    testdouts_3[150].write(v_.bit(3).sl());
    testdouts_4[150].write(v_.bit(4).sl());
    testdouts_5[150].write(v_.bit(5).sl());
    testdouts_6[150].write(v_.bit(6).sl());
    testdouts_7[150].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_1 - Lane 0 control
// OFFSET: 0x502
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] hsrxpon_if_lane0_rw - Power on of the RX HS
// RESET: 0x00
// [3] hsrxpon_bypass_lane0_rw - Power on of the RX HS bypass control
// RESET: 0x00
// [2] hsrxenterm_if_lane0_rw - RX HS enable term
// RESET: 0x00
// [1] hsrxenterm_bypass_lane0_rw - Bypass control of RX high speed enable term
// RESET: 0x00
// [0] hsrx_en_if_lane0_tester_rw - HS RX enable for tester
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_152() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x502ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(hsrxpon_if_lane0_rw), vl::rd(hsrxpon_bypass_lane0_rw), vl::rd(hsrxenterm_if_lane0_rw), vl::rd(hsrxenterm_bypass_lane0_rw), vl::rd(hsrx_en_if_lane0_tester_rw));
    testdouts_0[151].write(v_.bit(0).sl());
    testdouts_1[151].write(v_.bit(1).sl());
    testdouts_2[151].write(v_.bit(2).sl());
    testdouts_3[151].write(v_.bit(3).sl());
    testdouts_4[151].write(v_.bit(4).sl());
    testdouts_5[151].write(v_.bit(5).sl());
    testdouts_6[151].write(v_.bit(6).sl());
    testdouts_7[151].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_3 - Lane 0 control
// OFFSET: 0x504
// ATTR: RW, CUST
// FIELDS:
// [7] lprxponcd_bypass_lane0_rw - LP RX Contention Detector power-on bypass override
// RESET: 0x00
// [6:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_153() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x504ULL))) & vl::cat(vl::rd(lprxponcd_bypass_lane0_rw), vl::logic<7>(0x0ULL));
    testdouts_0[152].write(v_.bit(0).sl());
    testdouts_1[152].write(v_.bit(1).sl());
    testdouts_2[152].write(v_.bit(2).sl());
    testdouts_3[152].write(v_.bit(3).sl());
    testdouts_4[152].write(v_.bit(4).sl());
    testdouts_5[152].write(v_.bit(5).sl());
    testdouts_6[152].write(v_.bit(6).sl());
    testdouts_7[152].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_4 - Lane 0 control
// OFFSET: 0x505
// ATTR: RW, CUST
// FIELDS:
// [7:6] lptxdin_lane0_rw__1__0__ - LP TX Driver Data input, can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [5] lptxdin_bypass_lane0_rw - LP TX Driver Data input bypass override
// RESET: 0x00
// [4] lprxponulp_lane0_rw - LP RX ULP power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [3] lprxponulp_bypass_lane0_rw - LP RX ULP power-on bypass override
// RESET: 0x00
// [2] lprxponlp_lane0_rw - LP RX LP power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [1] lprxponlp_bypass_lane0_rw - LP RX LP power-on bypass override
// RESET: 0x00
// [0] lprxponcd_lane0_rw - LP RX Contention Detector power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_154() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x505ULL))) & vl::cat(vl::rd(lptxdin_lane0_rw), vl::rd(lptxdin_bypass_lane0_rw), vl::rd(lprxponulp_lane0_rw), vl::rd(lprxponulp_bypass_lane0_rw), vl::rd(lprxponlp_lane0_rw), vl::rd(lprxponlp_bypass_lane0_rw), vl::rd(lprxponcd_lane0_rw));
    testdouts_0[153].write(v_.bit(0).sl());
    testdouts_1[153].write(v_.bit(1).sl());
    testdouts_2[153].write(v_.bit(2).sl());
    testdouts_3[153].write(v_.bit(3).sl());
    testdouts_4[153].write(v_.bit(4).sl());
    testdouts_5[153].write(v_.bit(5).sl());
    testdouts_6[153].write(v_.bit(6).sl());
    testdouts_7[153].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_5 - Lane 0 control
// OFFSET: 0x506
// ATTR: RW, CUST
// FIELDS:
// [7] lptxpon_lane0_rw - LP TX power-on
// RESET: 0x00
// [6] lptxpon_bypass_lane0_rw - LP TX power-on bypass bit
// RESET: 0x00
// [5] lptxenulp_lane0_rw - LP TX enable ULP
// RESET: 0x00
// [4] lptxenulp_bypass_lane0_rw - LP TX enable ULP bypass bit
// RESET: 0x01
// [3] lptxenpulldwn_lane0_rw - LP TX enable pull-down
// RESET: 0x00
// [2] lptxenpulldwn_bypass_lane0_rw - LP TX enable pull-down bypass bit
// RESET: 0x00
// [1] lptxena_lane0_rw - LP TX enable
// RESET: 0x00
// [0] lptxena_bypass_lane0_rw - LP TX enable bypass bit
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_155() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL))) & vl::cat(vl::rd(lptxpon_lane0_rw), vl::rd(lptxpon_bypass_lane0_rw), vl::rd(lptxenulp_lane0_rw), vl::rd(lptxenulp_bypass_lane0_rw), vl::rd(lptxenpulldwn_lane0_rw), vl::rd(lptxenpulldwn_bypass_lane0_rw), vl::rd(lptxena_lane0_rw), vl::rd(lptxena_bypass_lane0_rw));
    testdouts_0[154].write(v_.bit(0).sl());
    testdouts_1[154].write(v_.bit(1).sl());
    testdouts_2[154].write(v_.bit(2).sl());
    testdouts_3[154].write(v_.bit(3).sl());
    testdouts_4[154].write(v_.bit(4).sl());
    testdouts_5[154].write(v_.bit(5).sl());
    testdouts_6[154].write(v_.bit(6).sl());
    testdouts_7[154].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_6 - Lane 0 control
// OFFSET: 0x507
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx0_rxhs_feed_int_clk_ovr_en_rw - HS-RX source clock override enable control for lane 0. Selects between internal clock source or external (receiver)
// RESET: 0x00
// [4] rx0_rxhs_feed_int_clk_ovr_rw - HS-RX source clock override control for lane 0. Selects between internal clock source or external (receiver)
// RESET: 0x00
// [3:1] reserved_1 - Reserved field
// RESET: 0x00
// [0] polarity_lane0_rw -
// NOTES: Polarity change control
// <br>0 - Two PHYs with P/N ports connected to P/N respectively
// <br>1 - Two PHYs with P/N ports connected to N/P respectively
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_156() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x507ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx0_rxhs_feed_int_clk_ovr_en_rw), vl::rd(rx0_rxhs_feed_int_clk_ovr_rw), vl::logic<3>(0x0ULL), vl::rd(polarity_lane0_rw));
    testdouts_0[155].write(v_.bit(0).sl());
    testdouts_1[155].write(v_.bit(1).sl());
    testdouts_2[155].write(v_.bit(2).sl());
    testdouts_3[155].write(v_.bit(3).sl());
    testdouts_4[155].write(v_.bit(4).sl());
    testdouts_5[155].write(v_.bit(5).sl());
    testdouts_6[155].write(v_.bit(6).sl());
    testdouts_7[155].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_7 - Lane 0 control
// OFFSET: 0x508
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx0_rxhs_clk_from_long_channel_if_rw - RX HS clock from long channel
// RESET: 0x00
// [4] recal_lane0_ovr_rw - Override Enable voltage control for recal
// RESET: 0x00
// [3] recal_lane0_ovr_en_rw - Override Enable voltage control for recal
// RESET: 0x00
// [2:0] reserved_1 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_157() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x508ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx0_rxhs_clk_from_long_channel_if_rw), vl::rd(recal_lane0_ovr_rw), vl::rd(recal_lane0_ovr_en_rw), vl::logic<3>(0x0ULL));
    testdouts_0[156].write(v_.bit(0).sl());
    testdouts_1[156].write(v_.bit(1).sl());
    testdouts_2[156].write(v_.bit(2).sl());
    testdouts_3[156].write(v_.bit(3).sl());
    testdouts_4[156].write(v_.bit(4).sl());
    testdouts_5[156].write(v_.bit(5).sl());
    testdouts_6[156].write(v_.bit(6).sl());
    testdouts_7[156].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_8 - Lane 0 control
// OFFSET: 0x509
// ATTR: RW, CUST
// FIELDS:
// [7] rx_des_en_lane0_rw - RX deserializer enable
// RESET: 0x00
// [6] rx_des_en_bypass_lane0_rw - RX deserializer enable bypass control
// RESET: 0x00
// [5] rx0_txlp_sr_bypass_if_rw - LP TX slew-rate bypass
// RESET: 0x00
// [4:3] rx0_txlp_eniocont_if_rw__1__0__ - RX IO continuity enable
// RESET: 0x00
// [2:0] rx0_rxhs_data_eq_if_rw__2__0__ - RX HS data equalizer
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_158() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x509ULL))) & vl::cat(vl::rd(rx_des_en_lane0_rw), vl::rd(rx_des_en_bypass_lane0_rw), vl::rd(rx0_txlp_sr_bypass_if_rw), vl::rd(rx0_txlp_eniocont_if_rw), vl::rd(rx0_rxhs_data_eq_if_rw));
    testdouts_0[157].write(v_.bit(0).sl());
    testdouts_1[157].write(v_.bit(1).sl());
    testdouts_2[157].write(v_.bit(2).sl());
    testdouts_3[157].write(v_.bit(3).sl());
    testdouts_4[157].write(v_.bit(4).sl());
    testdouts_5[157].write(v_.bit(5).sl());
    testdouts_6[157].write(v_.bit(6).sl());
    testdouts_7[157].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_9 - Lane 0 control
// OFFSET: 0x50A
// ATTR: RW, CUST
// FIELDS:
// [7] rx0_pm_sample_error_counter_rw - strobe to read the error counter
// RESET: 0x00
// [6] rx0_pm_start_control_ovr_en_rw - Pattern matcher start override enable.
// RESET: 0x00
// [5] rx0_pm_start_control_ovr_rw - Pattern matcher start override.
// RESET: 0x00
// [4:1] rx0_pm_mode_r_rw__3__0__ - Configuration mode for the pattern matcher
// RESET: 0x07
// [0] rx0_data_retiming_out_swap_sel_rw - retiming mux control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_159() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x50aULL))) & vl::cat(vl::rd(rx0_pm_sample_error_counter_rw), vl::rd(rx0_pm_start_control_ovr_en_rw), vl::rd(rx0_pm_start_control_ovr_rw), vl::rd(rx0_pm_mode_r_rw), vl::rd(rx0_data_retiming_out_swap_sel_rw));
    testdouts_0[158].write(v_.bit(0).sl());
    testdouts_1[158].write(v_.bit(1).sl());
    testdouts_2[158].write(v_.bit(2).sl());
    testdouts_3[158].write(v_.bit(3).sl());
    testdouts_4[158].write(v_.bit(4).sl());
    testdouts_5[158].write(v_.bit(5).sl());
    testdouts_6[158].write(v_.bit(6).sl());
    testdouts_7[158].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_LANE_12 - Lane 0 control
// OFFSET: 0x50D
// ATTR: RW, CUST
// FIELDS:
// [7] rx0_pm_man_error_latch_rw - Allows manual pattern matcher error counter latch. Latching is controlled by rx0_pm_sample_error_counter
// RESET: 0x00
// [6:3] rx0_lb_prog_if_rw__3__0__ - lane 0 receiver biasing currents
// RESET: 0x0F
// [2] rx0_lb_res_cm_en_if_rw - lane 0 static termination driver middle point pull down enable
// RESET: 0x00
// [1] rx0_lb_res_dn_en_if_rw - lane 0 static termination driver test dn line enable
// RESET: 0x00
// [0] rx0_lb_res_dp_en_if_rw - lane 0 static termination driver test dp line enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_160() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x50dULL))) & vl::cat(vl::rd(rx0_pm_man_error_latch_rw), vl::rd(rx0_lb_prog_if_rw), vl::rd(rx0_lb_res_cm_en_if_rw), vl::rd(rx0_lb_res_dn_en_if_rw), vl::rd(rx0_lb_res_dp_en_if_rw));
    testdouts_0[159].write(v_.bit(0).sl());
    testdouts_1[159].write(v_.bit(1).sl());
    testdouts_2[159].write(v_.bit(2).sl());
    testdouts_3[159].write(v_.bit(3).sl());
    testdouts_4[159].write(v_.bit(4).sl());
    testdouts_5[159].write(v_.bit(5).sl());
    testdouts_6[159].write(v_.bit(6).sl());
    testdouts_7[159].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_0 - Lane 0 observability
// OFFSET: 0x52B
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rx0_rxhs_deserial_data_en_if - HS-RX deserializer reset observation for lane 0
// RESET: 0x00
// [2] hsrx_en_if_lane0 - HS-RX enable of lane 0
// RESET: 0x00
// [1] enable_lane0 - Signal that passes AFE control from soft macro to lane 0
// RESET: 0x00
// [0] bintpon_if_lane0 - Level shifter enable of lane 0
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_161() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x52bULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx0_rxhs_deserial_data_en_if), vl::rd(hsrx_en_if_lane0), vl::rd(enable_lane0), vl::rd(bintpon_if_lane0));
    testdouts_0[160].write(v_.bit(0).sl());
    testdouts_1[160].write(v_.bit(1).sl());
    testdouts_2[160].write(v_.bit(2).sl());
    testdouts_3[160].write(v_.bit(3).sl());
    testdouts_4[160].write(v_.bit(4).sl());
    testdouts_5[160].write(v_.bit(5).sl());
    testdouts_6[160].write(v_.bit(6).sl());
    testdouts_7[160].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_1 - Lane 0 observability
// OFFSET: 0x52C
// ATTR: READ, CUST
// FIELDS:
// [7] lptxpon_if_lane0 - LP-TX power on
// RESET: 0x00
// [6] lptxenulp_if_lane0 - LP-TX pull up enable (for stopstate)
// RESET: 0x00
// [5] lptxenpulldn_if_lane0 - LP-TX pull down enable (for ULP)
// RESET: 0x00
// [4] lptxena_if_lane0 - LP-TX driver enable
// RESET: 0x00
// [3:2] lptxdin_if_lane0__1__0__ - LP-TX data in
// RESET: 0x00
// [1:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_162() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x52cULL))) & vl::cat(vl::rd(lptxpon_if_lane0), vl::rd(lptxenulp_if_lane0), vl::rd(lptxenpulldn_if_lane0), vl::rd(lptxena_if_lane0), vl::rd(lptxdin_if_lane0), vl::logic<2>(0x0ULL));
    testdouts_0[161].write(v_.bit(0).sl());
    testdouts_1[161].write(v_.bit(1).sl());
    testdouts_2[161].write(v_.bit(2).sl());
    testdouts_3[161].write(v_.bit(3).sl());
    testdouts_4[161].write(v_.bit(4).sl());
    testdouts_5[161].write(v_.bit(5).sl());
    testdouts_6[161].write(v_.bit(6).sl());
    testdouts_7[161].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_2 - Lane 0 observability
// OFFSET: 0x52D
// ATTR: READ, CUST
// FIELDS:
// [7] rx0_rxhs_rx_enterm_if - RX HS enable term
// RESET: 0x00
// [6] rx0_rxhs_rx_enlogic_if - RX HS enlogic
// RESET: 0x00
// [5] rx0_rxhs_rx_en_if - RX HS enable
// RESET: 0x00
// [4:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_163() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x52dULL))) & vl::cat(vl::rd(rx0_rxhs_rx_enterm_if), vl::rd(rx0_rxhs_rx_enlogic_if), vl::rd(rx0_rxhs_rx_en_if), vl::logic<5>(0x0ULL));
    testdouts_0[162].write(v_.bit(0).sl());
    testdouts_1[162].write(v_.bit(1).sl());
    testdouts_2[162].write(v_.bit(2).sl());
    testdouts_3[162].write(v_.bit(3).sl());
    testdouts_4[162].write(v_.bit(4).sl());
    testdouts_5[162].write(v_.bit(5).sl());
    testdouts_6[162].write(v_.bit(6).sl());
    testdouts_7[162].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_3 - Lane 0 observability
// OFFSET: 0x52E
// ATTR: READ, CUST
// FIELDS:
// [7:6] lprxdoutlp_lane0__1__0__ - LP-RX receiver outputs
// RESET: 0x00
// [5:4] lprxdoutcd_lane0__1__0__ - LP-CD receiver outputs
// RESET: 0x00
// [3:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] rx0_rxhs_rx_pon_if - RX HS power on
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_164() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x52eULL))) & vl::cat(vl::rd(lprxdoutlp_lane0), vl::rd(lprxdoutcd_lane0), vl::logic<3>(0x0ULL), vl::rd(rx0_rxhs_rx_pon_if));
    testdouts_0[163].write(v_.bit(0).sl());
    testdouts_1[163].write(v_.bit(1).sl());
    testdouts_2[163].write(v_.bit(2).sl());
    testdouts_3[163].write(v_.bit(3).sl());
    testdouts_4[163].write(v_.bit(4).sl());
    testdouts_5[163].write(v_.bit(5).sl());
    testdouts_6[163].write(v_.bit(6).sl());
    testdouts_7[163].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_4 - Lane 0 observability
// OFFSET: 0x52F
// ATTR: READ, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx0_deskewcalfailed_if - Deskew calibration failed signal
// RESET: 0x00
// [3] rx0_deskewcaldone_if - Deskew calibration don
// RESET: 0x00
// [2] lsrxclk_lane0 - Low speed clock (XOR'ed from data lines)
// RESET: 0x00
// [1:0] lprxdoutulp_lane0__1__0__ - LP-RX ULP receiver outputs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_165() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x52fULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx0_deskewcalfailed_if), vl::rd(rx0_deskewcaldone_if), vl::rd(lsrxclk_lane0), vl::rd(lprxdoutulp_lane0));
    testdouts_0[164].write(v_.bit(0).sl());
    testdouts_1[164].write(v_.bit(1).sl());
    testdouts_2[164].write(v_.bit(2).sl());
    testdouts_3[164].write(v_.bit(3).sl());
    testdouts_4[164].write(v_.bit(4).sl());
    testdouts_5[164].write(v_.bit(5).sl());
    testdouts_6[164].write(v_.bit(6).sl());
    testdouts_7[164].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_5 - Lane 0 observability
// OFFSET: 0x530
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx0_rxhs_data_if__7__0__ - RX HS data
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_166() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x530ULL))) & vl::rd(rx0_rxhs_data_if);
    testdouts_0[165].write(v_.bit(0).sl());
    testdouts_1[165].write(v_.bit(1).sl());
    testdouts_2[165].write(v_.bit(2).sl());
    testdouts_3[165].write(v_.bit(3).sl());
    testdouts_4[165].write(v_.bit(4).sl());
    testdouts_5[165].write(v_.bit(5).sl());
    testdouts_6[165].write(v_.bit(6).sl());
    testdouts_7[165].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_6 - Lane 0 observability
// OFFSET: 0x531
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx0_pm_error_pm - Observation of the error flag generated by the master
// RESET: 0x00
// [4:3] rx0_txlp_iocont_if__1__0__ - IO continuity receiver output
// RESET: 0x00
// [2] rx0_rxhs_word_clk_if - RX HS word clock
// RESET: 0x00
// [1] rx0_rxhs_feed_int_clk_if - RX HS internal clock feedback
// RESET: 0x00
// [0] rx0_rxhs_des_en_if - RX HS deserializer enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_167() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x531ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx0_pm_error_pm), vl::rd(rx0_txlp_iocont_if), vl::rd(rx0_rxhs_word_clk_if), vl::rd(rx0_rxhs_feed_int_clk_if), vl::rd(rx0_rxhs_des_en_if));
    testdouts_0[166].write(v_.bit(0).sl());
    testdouts_1[166].write(v_.bit(1).sl());
    testdouts_2[166].write(v_.bit(2).sl());
    testdouts_3[166].write(v_.bit(3).sl());
    testdouts_4[166].write(v_.bit(4).sl());
    testdouts_5[166].write(v_.bit(5).sl());
    testdouts_6[166].write(v_.bit(6).sl());
    testdouts_7[166].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_7 - Lane 0 observability
// OFFSET: 0x532
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx0_pm_error_counter__7__0__ - Error counter value
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_168() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x532ULL))) & vl::rd(rx0_pm_error_counter).slice<8>(0);
    testdouts_0[167].write(v_.bit(0).sl());
    testdouts_1[167].write(v_.bit(1).sl());
    testdouts_2[167].write(v_.bit(2).sl());
    testdouts_3[167].write(v_.bit(3).sl());
    testdouts_4[167].write(v_.bit(4).sl());
    testdouts_5[167].write(v_.bit(5).sl());
    testdouts_6[167].write(v_.bit(6).sl());
    testdouts_7[167].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_8 - Lane 0 observability
// OFFSET: 0x533
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx0_pm_error_counter__15__8__ - Error counter value
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_169() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x533ULL))) & vl::rd(rx0_pm_error_counter).slice<8>(8);
    testdouts_0[168].write(v_.bit(0).sl());
    testdouts_1[168].write(v_.bit(1).sl());
    testdouts_2[168].write(v_.bit(2).sl());
    testdouts_3[168].write(v_.bit(3).sl());
    testdouts_4[168].write(v_.bit(4).sl());
    testdouts_5[168].write(v_.bit(5).sl());
    testdouts_6[168].write(v_.bit(6).sl());
    testdouts_7[168].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_LANE_9 - Lane 0 observability
// OFFSET: 0x534
// ATTR: READ, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] rx0_pm_start - Pattern matcher start flag observation
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_170() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x534ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::logic<6>(0x0ULL), vl::rd(rx0_pm_start));
    testdouts_0[169].write(v_.bit(0).sl());
    testdouts_1[169].write(v_.bit(1).sl());
    testdouts_2[169].write(v_.bit(2).sl());
    testdouts_3[169].write(v_.bit(3).sl());
    testdouts_4[169].write(v_.bit(4).sl());
    testdouts_5[169].write(v_.bit(5).sl());
    testdouts_6[169].write(v_.bit(6).sl());
    testdouts_7[169].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_AFE_OBS_2 - Lane 0 observability
// OFFSET: 0x556
// ATTR: READ, CUST
// FIELDS:
// [7] tx0_txlp_enulp - LP-TX ULP enable
// RESET: 0x00
// [6] tx0_txlp_ena - LP-TX driver enable
// RESET: 0x00
// [5] tx0_txlp_enlogic - LP-TX level shifter enable
// RESET: 0x00
// [4] tx0_txlp_pon - LP-TX driver power on
// RESET: 0x00
// [3:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_171() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x556ULL))) & vl::cat(vl::rd(tx0_txlp_enulp), vl::rd(tx0_txlp_ena), vl::rd(tx0_txlp_enlogic), vl::rd(tx0_txlp_pon), vl::logic<4>(0x0ULL));
    testdouts_0[170].write(v_.bit(0).sl());
    testdouts_1[170].write(v_.bit(1).sl());
    testdouts_2[170].write(v_.bit(2).sl());
    testdouts_3[170].write(v_.bit(3).sl());
    testdouts_4[170].write(v_.bit(4).sl());
    testdouts_5[170].write(v_.bit(5).sl());
    testdouts_6[170].write(v_.bit(6).sl());
    testdouts_7[170].write(v_.bit(7).sl());
}

// %%CREG RD_TX_LANE0_AFE_OBS_3 - Lane 0 observability
// OFFSET: 0x557
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] tx0_rxlp_ponulp - LP-RX ULP receiver power on
// RESET: 0x00
// [5] tx0_rxlp_ponlp - LP-RX LP receiver power on
// RESET: 0x00
// [4] tx0_rxlp_poncd - LP-RX contention detector receiver power on
// RESET: 0x00
// [3] tx0_txlp_sr_bypass - HS-TX slew rate circuitry bypass
// RESET: 0x00
// [2] tx0_txlp_enpulldwn - LP-TX pull down enable
// RESET: 0x00
// [1:0] tx0_txlp_din__1__0__ - LP-TX driver data in
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_172() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x557ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(tx0_rxlp_ponulp), vl::rd(tx0_rxlp_ponlp), vl::rd(tx0_rxlp_poncd), vl::rd(tx0_txlp_sr_bypass), vl::rd(tx0_txlp_enpulldwn), vl::rd(tx0_txlp_din));
    testdouts_0[171].write(v_.bit(0).sl());
    testdouts_1[171].write(v_.bit(1).sl());
    testdouts_2[171].write(v_.bit(2).sl());
    testdouts_3[171].write(v_.bit(3).sl());
    testdouts_4[171].write(v_.bit(4).sl());
    testdouts_5[171].write(v_.bit(5).sl());
    testdouts_6[171].write(v_.bit(6).sl());
    testdouts_7[171].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_OFFSET_CAL_0 - Lane 0 offset compensation control
// OFFSET: 0x57B
// ATTR: RW, CUST
// FIELDS:
// [7] rx0_rxhs_data_offset_cal_en_ovr_en_rw - Data offset calibration override enable control
// RESET: 0x00
// [6] rx0_cal_done_ovr_rw - Calibration done override enable control
// RESET: 0x00
// [5] rx0_cal_done_ovr_en_rw - Calibration done override enable control
// RESET: 0x00
// [4] rx0_start_calibration_ovr_rw - RX start calibration
// RESET: 0x00
// [3] rx0_start_calibration_ovr_en_rw - RX start calibration
// RESET: 0x00
// [2:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_173() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x57bULL))) & vl::cat(vl::rd(rx0_rxhs_data_offset_cal_en_ovr_en_rw), vl::rd(rx0_cal_done_ovr_rw), vl::rd(rx0_cal_done_ovr_en_rw), vl::rd(rx0_start_calibration_ovr_rw), vl::rd(rx0_start_calibration_ovr_en_rw), vl::logic<3>(0x0ULL));
    testdouts_0[172].write(v_.bit(0).sl());
    testdouts_1[172].write(v_.bit(1).sl());
    testdouts_2[172].write(v_.bit(2).sl());
    testdouts_3[172].write(v_.bit(3).sl());
    testdouts_4[172].write(v_.bit(4).sl());
    testdouts_5[172].write(v_.bit(5).sl());
    testdouts_6[172].write(v_.bit(6).sl());
    testdouts_7[172].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_OFFSET_CAL_1 - Lane 0 offset compensation control
// OFFSET: 0x57C
// ATTR: RW, CUST
// FIELDS:
// [7] rx0_rxhs_data_offset_cal_ovr_en_rw - Offset calibration setting override
// RESET: 0x00
// [6:1] rx0_rxhs_data_offset_cal_ovr_rw__5__0__ - Offset calibration setting override enable
// RESET: 0x00
// [0] rx0_rxhs_data_offset_cal_en_ovr_rw - Data offset calibration override enable control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_174() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x57cULL))) & vl::cat(vl::rd(rx0_rxhs_data_offset_cal_ovr_en_rw), vl::rd(rx0_rxhs_data_offset_cal_ovr_rw), vl::rd(rx0_rxhs_data_offset_cal_en_ovr_rw));
    testdouts_0[173].write(v_.bit(0).sl());
    testdouts_1[173].write(v_.bit(1).sl());
    testdouts_2[173].write(v_.bit(2).sl());
    testdouts_3[173].write(v_.bit(3).sl());
    testdouts_4[173].write(v_.bit(4).sl());
    testdouts_5[173].write(v_.bit(5).sl());
    testdouts_6[173].write(v_.bit(6).sl());
    testdouts_7[173].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_OFFSET_CAL_2 - Lane 0 offset compensation control
// OFFSET: 0x57D
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx0_rxhs_data_ovr_rw__7__0__ - RX HS data override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_175() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x57dULL))) & vl::rd(rx0_rxhs_data_ovr_rw);
    testdouts_0[174].write(v_.bit(0).sl());
    testdouts_1[174].write(v_.bit(1).sl());
    testdouts_2[174].write(v_.bit(2).sl());
    testdouts_3[174].write(v_.bit(3).sl());
    testdouts_4[174].write(v_.bit(4).sl());
    testdouts_5[174].write(v_.bit(5).sl());
    testdouts_6[174].write(v_.bit(6).sl());
    testdouts_7[174].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_OFFSET_CAL_3 - Lane 0 offset compensation control
// OFFSET: 0x57E
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] rx0_rxhs_repeatcal_rw - RX HS repeat calibration
// RESET: 0x00
// [0] rx0_rxhs_data_ovr_en_rw - RX HS data override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_176() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x57eULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(rx0_rxhs_repeatcal_rw), vl::rd(rx0_rxhs_data_ovr_en_rw));
    testdouts_0[175].write(v_.bit(0).sl());
    testdouts_1[175].write(v_.bit(1).sl());
    testdouts_2[175].write(v_.bit(2).sl());
    testdouts_3[175].write(v_.bit(3).sl());
    testdouts_4[175].write(v_.bit(4).sl());
    testdouts_5[175].write(v_.bit(5).sl());
    testdouts_6[175].write(v_.bit(6).sl());
    testdouts_7[175].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_OFFSET_CAL_0 - Lane 0 offset compensation observability
// OFFSET: 0x59F
// ATTR: READ, CUST
// FIELDS:
// [7:3] reserved_0 - Reserved field
// RESET: 0x00
// [2] rx0_cal_error - Offset calibration algorithm error
// RESET: 0x00
// [1] rx0_cal_done - Offset calibration algorithm done
// RESET: 0x00
// [0] rx0_start_calibration - Offset calibration algorithm start
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_177() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x59fULL))) & vl::cat(vl::logic<5>(0x0ULL), vl::rd(rx0_cal_error), vl::rd(rx0_cal_done), vl::rd(rx0_start_calibration));
    testdouts_0[176].write(v_.bit(0).sl());
    testdouts_1[176].write(v_.bit(1).sl());
    testdouts_2[176].write(v_.bit(2).sl());
    testdouts_3[176].write(v_.bit(3).sl());
    testdouts_4[176].write(v_.bit(4).sl());
    testdouts_5[176].write(v_.bit(5).sl());
    testdouts_6[176].write(v_.bit(6).sl());
    testdouts_7[176].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_OFFSET_CAL_1 - Lane 0 offset compensation observability
// OFFSET: 0x5A0
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx0_rxhs_data_int__7__0__ - HS-RX data received (AFE output)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_178() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5a0ULL))) & vl::rd(rx0_rxhs_data_int);
    testdouts_0[177].write(v_.bit(0).sl());
    testdouts_1[177].write(v_.bit(1).sl());
    testdouts_2[177].write(v_.bit(2).sl());
    testdouts_3[177].write(v_.bit(3).sl());
    testdouts_4[177].write(v_.bit(4).sl());
    testdouts_5[177].write(v_.bit(5).sl());
    testdouts_6[177].write(v_.bit(6).sl());
    testdouts_7[177].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_OFFSET_CAL_2 - Lane 0 offset compensation observability
// OFFSET: 0x5A1
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] rx0_rxhs_data_offset_cal_en - Offset calibration algorithm enable
// RESET: 0x00
// [5:0] rx0_rxhs_data_offset_cal__5__0__ - Offset calibration algorithm setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_179() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5a1ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx0_rxhs_data_offset_cal_en), vl::rd(rx0_rxhs_data_offset_cal));
    testdouts_0[178].write(v_.bit(0).sl());
    testdouts_1[178].write(v_.bit(1).sl());
    testdouts_2[178].write(v_.bit(2).sl());
    testdouts_3[178].write(v_.bit(3).sl());
    testdouts_4[178].write(v_.bit(4).sl());
    testdouts_5[178].write(v_.bit(5).sl());
    testdouts_6[178].write(v_.bit(6).sl());
    testdouts_7[178].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_OFFSET_CAL_3 - Lane 0 offset compensation observability
// OFFSET: 0x5A2
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx0_total_ones__7__0__ - Offset calibration algorithm counter of 1s
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_180() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5a2ULL))) & vl::rd(rx0_total_ones);
    testdouts_0[179].write(v_.bit(0).sl());
    testdouts_1[179].write(v_.bit(1).sl());
    testdouts_2[179].write(v_.bit(2).sl());
    testdouts_3[179].write(v_.bit(3).sl());
    testdouts_4[179].write(v_.bit(4).sl());
    testdouts_5[179].write(v_.bit(5).sl());
    testdouts_6[179].write(v_.bit(6).sl());
    testdouts_7[179].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_DDL_0 - Lane 0 DDL tune observability
// OFFSET: 0x5E0
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rx0_ddl_start_calibration_if - DDL tune digital algorithm start
// RESET: 0x00
// [2] rx0_ddl_finished_if - DDL tune done indication
// RESET: 0x00
// [1] rx0_ddl_error_flag_if - DDL tune error
// RESET: 0x00
// [0] ddl_tune_en_lane0_if - DDL tune analog circuitry enable indication
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_181() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5e0ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx0_ddl_start_calibration_if), vl::rd(rx0_ddl_finished_if), vl::rd(rx0_ddl_error_flag_if), vl::rd(ddl_tune_en_lane0_if));
    testdouts_0[180].write(v_.bit(0).sl());
    testdouts_1[180].write(v_.bit(1).sl());
    testdouts_2[180].write(v_.bit(2).sl());
    testdouts_3[180].write(v_.bit(3).sl());
    testdouts_4[180].write(v_.bit(4).sl());
    testdouts_5[180].write(v_.bit(5).sl());
    testdouts_6[180].write(v_.bit(6).sl());
    testdouts_7[180].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_DDL_1 - Lane 0 DDL tune observability
// OFFSET: 0x5E1
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx0_freq_meas_out_if__7__0__ - RX DDL tunning oscillation measurement
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_182() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5e1ULL))) & vl::rd(rx0_freq_meas_out_if).slice<8>(0);
    testdouts_0[181].write(v_.bit(0).sl());
    testdouts_1[181].write(v_.bit(1).sl());
    testdouts_2[181].write(v_.bit(2).sl());
    testdouts_3[181].write(v_.bit(3).sl());
    testdouts_4[181].write(v_.bit(4).sl());
    testdouts_5[181].write(v_.bit(5).sl());
    testdouts_6[181].write(v_.bit(6).sl());
    testdouts_7[181].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_DDL_2 - Lane 0 DDL tune observability
// OFFSET: 0x5E2
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx0_freq_meas_out_if__11__8__ - RX DDL tunning oscillation measurement
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_183() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5e2ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx0_freq_meas_out_if).slice<4>(8));
    testdouts_0[182].write(v_.bit(0).sl());
    testdouts_1[182].write(v_.bit(1).sl());
    testdouts_2[182].write(v_.bit(2).sl());
    testdouts_3[182].write(v_.bit(3).sl());
    testdouts_4[182].write(v_.bit(4).sl());
    testdouts_5[182].write(v_.bit(5).sl());
    testdouts_6[182].write(v_.bit(6).sl());
    testdouts_7[182].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_DDL_3 - Lane 0 DDL tune observability
// OFFSET: 0x5E3
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx0_osc_freq_target_if__7__0__ - RX oscillator frequency target obs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_184() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5e3ULL))) & vl::rd(rx0_osc_freq_target_if).slice<8>(0);
    testdouts_0[183].write(v_.bit(0).sl());
    testdouts_1[183].write(v_.bit(1).sl());
    testdouts_2[183].write(v_.bit(2).sl());
    testdouts_3[183].write(v_.bit(3).sl());
    testdouts_4[183].write(v_.bit(4).sl());
    testdouts_5[183].write(v_.bit(5).sl());
    testdouts_6[183].write(v_.bit(6).sl());
    testdouts_7[183].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_DDL_4 - Lane 0 DDL tune observability
// OFFSET: 0x5E4
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx0_osc_freq_target_if__11__8__ - RX oscillator frequency target obs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_185() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5e4ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx0_osc_freq_target_if).slice<4>(8));
    testdouts_0[184].write(v_.bit(0).sl());
    testdouts_1[184].write(v_.bit(1).sl());
    testdouts_2[184].write(v_.bit(2).sl());
    testdouts_3[184].write(v_.bit(3).sl());
    testdouts_4[184].write(v_.bit(4).sl());
    testdouts_5[184].write(v_.bit(5).sl());
    testdouts_6[184].write(v_.bit(6).sl());
    testdouts_7[184].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_DDL_5 - Lane 0 DDL tune observability
// OFFSET: 0x5E5
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx0_rxhs_ddl_phase_change_p_if - RX HS DDL phase change
// RESET: 0x00
// [4:0] rx0_rxhs_ddl_tune_if__4__0__ - RX HS DDL tunning setting
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_186() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5e5ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx0_rxhs_ddl_phase_change_p_if), vl::rd(rx0_rxhs_ddl_tune_if));
    testdouts_0[185].write(v_.bit(0).sl());
    testdouts_1[185].write(v_.bit(1).sl());
    testdouts_2[185].write(v_.bit(2).sl());
    testdouts_3[185].write(v_.bit(3).sl());
    testdouts_4[185].write(v_.bit(4).sl());
    testdouts_5[185].write(v_.bit(5).sl());
    testdouts_6[185].write(v_.bit(6).sl());
    testdouts_7[185].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_DDL_6 - Lane 0 DDL tune observability
// OFFSET: 0x5E6
// ATTR: READ, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4:0] rx0_rxhs_ddl_phase_clk_if__4__0__ - Clock DDL setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_187() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5e6ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx0_rxhs_ddl_phase_clk_if));
    testdouts_0[186].write(v_.bit(0).sl());
    testdouts_1[186].write(v_.bit(1).sl());
    testdouts_2[186].write(v_.bit(2).sl());
    testdouts_3[186].write(v_.bit(3).sl());
    testdouts_4[186].write(v_.bit(4).sl());
    testdouts_5[186].write(v_.bit(5).sl());
    testdouts_6[186].write(v_.bit(6).sl());
    testdouts_7[186].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE0_DDL_7 - Lane 0 DDL tune observability
// OFFSET: 0x5E7
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx0_rxhs_ddl_phase_data_if__3__0__ - Data DDL setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_188() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x5e7ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx0_rxhs_ddl_phase_data_if));
    testdouts_0[187].write(v_.bit(0).sl());
    testdouts_1[187].write(v_.bit(1).sl());
    testdouts_2[187].write(v_.bit(2).sl());
    testdouts_3[187].write(v_.bit(3).sl());
    testdouts_4[187].write(v_.bit(4).sl());
    testdouts_5[187].write(v_.bit(5).sl());
    testdouts_6[187].write(v_.bit(6).sl());
    testdouts_7[187].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_DDL_0 - Lane 0 DDL tune control
// OFFSET: 0x606
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx0_rxhs_ddl_tune_en_if_rw - RX HS DDL tune enable
// RESET: 0x00
// [4] rx0_rxhs_ddl_tune_en_bypass_rw - RX HS DDL tune enable bypass
// RESET: 0x00
// [3] rx0_ddl_start_calibration_ovr_rw - RX DDL tunning calibration start override
// RESET: 0x00
// [2] rx0_ddl_start_calibration_ovr_en_rw - RX DDL tunning calibration start override enable
// RESET: 0x00
// [1] rx0_ddl_finished_ovr_rw - RX DDL tunning done override
// RESET: 0x00
// [0] rx0_ddl_finished_ovr_en_rw - RX DDL tunning done override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_189() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x606ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx0_rxhs_ddl_tune_en_if_rw), vl::rd(rx0_rxhs_ddl_tune_en_bypass_rw), vl::rd(rx0_ddl_start_calibration_ovr_rw), vl::rd(rx0_ddl_start_calibration_ovr_en_rw), vl::rd(rx0_ddl_finished_ovr_rw), vl::rd(rx0_ddl_finished_ovr_en_rw));
    testdouts_0[188].write(v_.bit(0).sl());
    testdouts_1[188].write(v_.bit(1).sl());
    testdouts_2[188].write(v_.bit(2).sl());
    testdouts_3[188].write(v_.bit(3).sl());
    testdouts_4[188].write(v_.bit(4).sl());
    testdouts_5[188].write(v_.bit(5).sl());
    testdouts_6[188].write(v_.bit(6).sl());
    testdouts_7[188].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_DDL_1 - Lane 0 DDL tune control
// OFFSET: 0x607
// ATTR: RW, CUST
// FIELDS:
// [7] rx0_rxhs_ddl_phase_change_p_ovr_rw - RX HS DDL phase changed override control
// RESET: 0x00
// [6] rx0_rxhs_ddl_phase_change_p_ovr_en_rw - RX HS DDL phase changed override control
// RESET: 0x00
// [5] rx0_rxhs_ddl_tune_ovr_en_rw - RX HS DDL tune override enable control
// RESET: 0x00
// [4:0] rx0_rxhs_ddl_tune_ovr_rw__4__0__ - RX HS DDL tune override for lane 0
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_190() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x607ULL))) & vl::cat(vl::rd(rx0_rxhs_ddl_phase_change_p_ovr_rw), vl::rd(rx0_rxhs_ddl_phase_change_p_ovr_en_rw), vl::rd(rx0_rxhs_ddl_tune_ovr_en_rw), vl::rd(rx0_rxhs_ddl_tune_ovr_rw));
    testdouts_0[189].write(v_.bit(0).sl());
    testdouts_1[189].write(v_.bit(1).sl());
    testdouts_2[189].write(v_.bit(2).sl());
    testdouts_3[189].write(v_.bit(3).sl());
    testdouts_4[189].write(v_.bit(4).sl());
    testdouts_5[189].write(v_.bit(5).sl());
    testdouts_6[189].write(v_.bit(6).sl());
    testdouts_7[189].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_DDL_2 - Lane 0 DDL tune control
// OFFSET: 0x608
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx0_rxhs_ddl_phase_clk_ovr_en_rw - RX HS DDL phase clock override control
// RESET: 0x00
// [4:0] rx0_rxhs_ddl_phase_clk_ovr_rw__4__0__ - RX HS DDL phase clock override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_191() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x608ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx0_rxhs_ddl_phase_clk_ovr_en_rw), vl::rd(rx0_rxhs_ddl_phase_clk_ovr_rw));
    testdouts_0[190].write(v_.bit(0).sl());
    testdouts_1[190].write(v_.bit(1).sl());
    testdouts_2[190].write(v_.bit(2).sl());
    testdouts_3[190].write(v_.bit(3).sl());
    testdouts_4[190].write(v_.bit(4).sl());
    testdouts_5[190].write(v_.bit(5).sl());
    testdouts_6[190].write(v_.bit(6).sl());
    testdouts_7[190].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_DDL_3 - Lane 0 DDL tune control
// OFFSET: 0x609
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx0_rxhs_ddl_phase_data_ovr_en_rw - Override enable for ddl phase data
// RESET: 0x00
// [3:0] rx0_rxhs_ddl_phase_data_ovr_rw__3__0__ - RX HS DDL phase data override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_192() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x609ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx0_rxhs_ddl_phase_data_ovr_en_rw), vl::rd(rx0_rxhs_ddl_phase_data_ovr_rw));
    testdouts_0[191].write(v_.bit(0).sl());
    testdouts_1[191].write(v_.bit(1).sl());
    testdouts_2[191].write(v_.bit(2).sl());
    testdouts_3[191].write(v_.bit(3).sl());
    testdouts_4[191].write(v_.bit(4).sl());
    testdouts_5[191].write(v_.bit(5).sl());
    testdouts_6[191].write(v_.bit(6).sl());
    testdouts_7[191].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_DDL_4 - Lane 0 DDL tune control
// OFFSET: 0x60A
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx0_osc_freq_target_if_ovr_rw__7__0__ - Oscillating frequency override Target
// RESET: 0x77
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_193() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x60aULL))) & vl::rd(rx0_osc_freq_target_if_ovr_rw).slice<8>(0);
    testdouts_0[192].write(v_.bit(0).sl());
    testdouts_1[192].write(v_.bit(1).sl());
    testdouts_2[192].write(v_.bit(2).sl());
    testdouts_3[192].write(v_.bit(3).sl());
    testdouts_4[192].write(v_.bit(4).sl());
    testdouts_5[192].write(v_.bit(5).sl());
    testdouts_6[192].write(v_.bit(6).sl());
    testdouts_7[192].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_DDL_5 - Lane 0 DDL tune control
// OFFSET: 0x60B
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx0_osc_freq_target_if_ovr_rw__11__8__ - Oscillating frequency override Target
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_194() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x60bULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx0_osc_freq_target_if_ovr_rw).slice<4>(8));
    testdouts_0[193].write(v_.bit(0).sl());
    testdouts_1[193].write(v_.bit(1).sl());
    testdouts_2[193].write(v_.bit(2).sl());
    testdouts_3[193].write(v_.bit(3).sl());
    testdouts_4[193].write(v_.bit(4).sl());
    testdouts_5[193].write(v_.bit(5).sl());
    testdouts_6[193].write(v_.bit(6).sl());
    testdouts_7[193].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE0_DDL_6 - Lane 0 DDL tune control
// OFFSET: 0x60C
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] rx0_osc_freq_target_if_ovr_en_rw - Override for oscillation frequency target
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_195() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x60cULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(rx0_osc_freq_target_if_ovr_en_rw));
    testdouts_0[194].write(v_.bit(0).sl());
    testdouts_1[194].write(v_.bit(1).sl());
    testdouts_2[194].write(v_.bit(2).sl());
    testdouts_3[194].write(v_.bit(3).sl());
    testdouts_4[194].write(v_.bit(4).sl());
    testdouts_5[194].write(v_.bit(5).sl());
    testdouts_6[194].write(v_.bit(6).sl());
    testdouts_7[194].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_0 - Lane 1 control
// OFFSET: 0x701
// ATTR: RW, CUST
// FIELDS:
// [7] hsrx_en_bypass_lane1_rw - HS RX enable bypass
// RESET: 0x00
// [6] bintpon_lane1_rw - Bias block power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [5] bintpon_bypass_lane1_rw - Bias block power-on bypass override
// RESET: 0x00
// [4:1] atb_sel_lane1_rw__3__0__ - Analog Test Bus signals selection:
// RESET: 0x00
// [0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_196() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x701ULL))) & vl::cat(vl::rd(hsrx_en_bypass_lane1_rw), vl::rd(bintpon_lane1_rw), vl::rd(bintpon_bypass_lane1_rw), vl::rd(atb_sel_lane1_rw), vl::logic<1>(0x0ULL));
    testdouts_0[195].write(v_.bit(0).sl());
    testdouts_1[195].write(v_.bit(1).sl());
    testdouts_2[195].write(v_.bit(2).sl());
    testdouts_3[195].write(v_.bit(3).sl());
    testdouts_4[195].write(v_.bit(4).sl());
    testdouts_5[195].write(v_.bit(5).sl());
    testdouts_6[195].write(v_.bit(6).sl());
    testdouts_7[195].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_1 - Lane 1 control
// OFFSET: 0x702
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] hsrxpon_if_lane1_rw - Power on of the RX HS
// RESET: 0x00
// [3] hsrxpon_bypass_lane1_rw - Power on of the RX HS bypass control
// RESET: 0x00
// [2] hsrxenterm_if_lane1_rw - RX HS enable term
// RESET: 0x00
// [1] hsrxenterm_bypass_lane1_rw - Bypass control of RX high speed enable term
// RESET: 0x00
// [0] hsrx_en_if_lane1_tester_rw - HS RX enable for tester
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_197() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x702ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(hsrxpon_if_lane1_rw), vl::rd(hsrxpon_bypass_lane1_rw), vl::rd(hsrxenterm_if_lane1_rw), vl::rd(hsrxenterm_bypass_lane1_rw), vl::rd(hsrx_en_if_lane1_tester_rw));
    testdouts_0[196].write(v_.bit(0).sl());
    testdouts_1[196].write(v_.bit(1).sl());
    testdouts_2[196].write(v_.bit(2).sl());
    testdouts_3[196].write(v_.bit(3).sl());
    testdouts_4[196].write(v_.bit(4).sl());
    testdouts_5[196].write(v_.bit(5).sl());
    testdouts_6[196].write(v_.bit(6).sl());
    testdouts_7[196].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_3 - Lane 1 control
// OFFSET: 0x704
// ATTR: RW, CUST
// FIELDS:
// [7] lprxponcd_bypass_lane1_rw - LP RX Contention Detector power-on bypass override
// RESET: 0x00
// [6:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_198() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x704ULL))) & vl::cat(vl::rd(lprxponcd_bypass_lane1_rw), vl::logic<7>(0x0ULL));
    testdouts_0[197].write(v_.bit(0).sl());
    testdouts_1[197].write(v_.bit(1).sl());
    testdouts_2[197].write(v_.bit(2).sl());
    testdouts_3[197].write(v_.bit(3).sl());
    testdouts_4[197].write(v_.bit(4).sl());
    testdouts_5[197].write(v_.bit(5).sl());
    testdouts_6[197].write(v_.bit(6).sl());
    testdouts_7[197].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_4 - Lane 1 control
// OFFSET: 0x705
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] lprxponulp_lane1_rw - LP RX ULP power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [3] lprxponulp_bypass_lane1_rw - LP RX ULP power-on bypass override
// RESET: 0x00
// [2] lprxponlp_lane1_rw - LP RX LP power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [1] lprxponlp_bypass_lane1_rw - LP RX LP power-on bypass override
// RESET: 0x00
// [0] lprxponcd_lane1_rw - LP RX Contention Detector power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_199() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x705ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(lprxponulp_lane1_rw), vl::rd(lprxponulp_bypass_lane1_rw), vl::rd(lprxponlp_lane1_rw), vl::rd(lprxponlp_bypass_lane1_rw), vl::rd(lprxponcd_lane1_rw));
    testdouts_0[198].write(v_.bit(0).sl());
    testdouts_1[198].write(v_.bit(1).sl());
    testdouts_2[198].write(v_.bit(2).sl());
    testdouts_3[198].write(v_.bit(3).sl());
    testdouts_4[198].write(v_.bit(4).sl());
    testdouts_5[198].write(v_.bit(5).sl());
    testdouts_6[198].write(v_.bit(6).sl());
    testdouts_7[198].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_6 - Lane 1 control
// OFFSET: 0x707
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx1_rxhs_feed_int_clk_ovr_en_rw - HS-RX source clock override enable control for lane 1. Selects between internal clock source or external (receiver)
// RESET: 0x00
// [4] rx1_rxhs_feed_int_clk_ovr_rw - HS-RX source clock override control for lane 1. Selects between internal clock source or external (receiver)
// RESET: 0x00
// [3:1] reserved_1 - Reserved field
// RESET: 0x00
// [0] polarity_lane1_rw -
// NOTES: Polarity change control
// <br>0 - Two PHYs with P/N ports connected to P/N respectively
// <br>1 - Two PHYs with P/N ports connected to N/P respectively
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_200() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x707ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx1_rxhs_feed_int_clk_ovr_en_rw), vl::rd(rx1_rxhs_feed_int_clk_ovr_rw), vl::logic<3>(0x0ULL), vl::rd(polarity_lane1_rw));
    testdouts_0[199].write(v_.bit(0).sl());
    testdouts_1[199].write(v_.bit(1).sl());
    testdouts_2[199].write(v_.bit(2).sl());
    testdouts_3[199].write(v_.bit(3).sl());
    testdouts_4[199].write(v_.bit(4).sl());
    testdouts_5[199].write(v_.bit(5).sl());
    testdouts_6[199].write(v_.bit(6).sl());
    testdouts_7[199].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_7 - Lane 1 control
// OFFSET: 0x708
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx1_rxhs_clk_from_long_channel_if_rw - RX HS clock from long channel
// RESET: 0x00
// [4] recal_lane1_ovr_rw - Override Enable voltage control for recal
// RESET: 0x00
// [3] recal_lane1_ovr_en_rw - Override Enable voltage control for recal
// RESET: 0x00
// [2:0] reserved_1 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_201() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x708ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx1_rxhs_clk_from_long_channel_if_rw), vl::rd(recal_lane1_ovr_rw), vl::rd(recal_lane1_ovr_en_rw), vl::logic<3>(0x0ULL));
    testdouts_0[200].write(v_.bit(0).sl());
    testdouts_1[200].write(v_.bit(1).sl());
    testdouts_2[200].write(v_.bit(2).sl());
    testdouts_3[200].write(v_.bit(3).sl());
    testdouts_4[200].write(v_.bit(4).sl());
    testdouts_5[200].write(v_.bit(5).sl());
    testdouts_6[200].write(v_.bit(6).sl());
    testdouts_7[200].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_8 - Lane 1 control
// OFFSET: 0x709
// ATTR: RW, CUST
// FIELDS:
// [7] rx_des_en_lane1_rw - RX deserializer enable
// RESET: 0x00
// [6] rx_des_en_bypass_lane1_rw - RX deserializer enable bypass control
// RESET: 0x00
// [5:3] reserved_0 - Reserved field
// RESET: 0x00
// [2:0] rx1_rxhs_data_eq_if_rw__2__0__ - RX HS data equalizer
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_202() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x709ULL))) & vl::cat(vl::rd(rx_des_en_lane1_rw), vl::rd(rx_des_en_bypass_lane1_rw), vl::logic<3>(0x0ULL), vl::rd(rx1_rxhs_data_eq_if_rw));
    testdouts_0[201].write(v_.bit(0).sl());
    testdouts_1[201].write(v_.bit(1).sl());
    testdouts_2[201].write(v_.bit(2).sl());
    testdouts_3[201].write(v_.bit(3).sl());
    testdouts_4[201].write(v_.bit(4).sl());
    testdouts_5[201].write(v_.bit(5).sl());
    testdouts_6[201].write(v_.bit(6).sl());
    testdouts_7[201].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_9 - Lane 1 control
// OFFSET: 0x70A
// ATTR: RW, CUST
// FIELDS:
// [7] rx1_pm_sample_error_counter_rw - strobe to read the error counter
// RESET: 0x00
// [6] rx1_pm_start_control_ovr_en_rw - Pattern matcher start override enable.
// RESET: 0x00
// [5] rx1_pm_start_control_ovr_rw - Pattern matcher start override.
// RESET: 0x00
// [4:1] rx1_pm_mode_r_rw__3__0__ - Configuration mode for the pattern matcher
// RESET: 0x07
// [0] rx1_data_retiming_out_swap_sel_rw - retiming mux control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_203() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x70aULL))) & vl::cat(vl::rd(rx1_pm_sample_error_counter_rw), vl::rd(rx1_pm_start_control_ovr_en_rw), vl::rd(rx1_pm_start_control_ovr_rw), vl::rd(rx1_pm_mode_r_rw), vl::rd(rx1_data_retiming_out_swap_sel_rw));
    testdouts_0[202].write(v_.bit(0).sl());
    testdouts_1[202].write(v_.bit(1).sl());
    testdouts_2[202].write(v_.bit(2).sl());
    testdouts_3[202].write(v_.bit(3).sl());
    testdouts_4[202].write(v_.bit(4).sl());
    testdouts_5[202].write(v_.bit(5).sl());
    testdouts_6[202].write(v_.bit(6).sl());
    testdouts_7[202].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_10 - Lane 1 control
// OFFSET: 0x70B
// ATTR: RW, CUST
// FIELDS:
// [7] rx1_rxhs_ddl_phase_clk_ovr_en_rw - RX HS DDL phase data override control
// RESET: 0x00
// [6:2] rx1_rxhs_ddl_phase_clk_ovr_rw__4__0__ - RX HS DDL phase clock override
// RESET: 0x00
// [1] rx1_rxhs_ddl_phase_change_p_ovr_en_rw - RX HS DDL phase changed override control
// RESET: 0x00
// [0] rx1_rxhs_ddl_phase_change_p_ovr_rw - RX HS DDL phase changed override control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_204() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x70bULL))) & vl::cat(vl::rd(rx1_rxhs_ddl_phase_clk_ovr_en_rw), vl::rd(rx1_rxhs_ddl_phase_clk_ovr_rw), vl::rd(rx1_rxhs_ddl_phase_change_p_ovr_en_rw), vl::rd(rx1_rxhs_ddl_phase_change_p_ovr_rw));
    testdouts_0[203].write(v_.bit(0).sl());
    testdouts_1[203].write(v_.bit(1).sl());
    testdouts_2[203].write(v_.bit(2).sl());
    testdouts_3[203].write(v_.bit(3).sl());
    testdouts_4[203].write(v_.bit(4).sl());
    testdouts_5[203].write(v_.bit(5).sl());
    testdouts_6[203].write(v_.bit(6).sl());
    testdouts_7[203].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_11 - Lane 1 control
// OFFSET: 0x70C
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx1_rxhs_ddl_phase_data_ovr_en_rw - Override enable for ddl phase data
// RESET: 0x00
// [3:0] rx1_rxhs_ddl_phase_data_ovr_rw__3__0__ - RX HS DDL phase clock override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_205() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x70cULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx1_rxhs_ddl_phase_data_ovr_en_rw), vl::rd(rx1_rxhs_ddl_phase_data_ovr_rw));
    testdouts_0[204].write(v_.bit(0).sl());
    testdouts_1[204].write(v_.bit(1).sl());
    testdouts_2[204].write(v_.bit(2).sl());
    testdouts_3[204].write(v_.bit(3).sl());
    testdouts_4[204].write(v_.bit(4).sl());
    testdouts_5[204].write(v_.bit(5).sl());
    testdouts_6[204].write(v_.bit(6).sl());
    testdouts_7[204].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_LANE_12 - Lane 1 control
// OFFSET: 0x70D
// ATTR: RW, CUST
// FIELDS:
// [7] rx1_pm_man_error_latch_rw - Allows manual pattern matcher error counter latch. Latching is controlled by rx1_pm_sample_error_counter
// RESET: 0x00
// [6:3] rx1_lb_prog_if_rw__3__0__ - lane 1 receiver biasing currents
// RESET: 0x0F
// [2] rx1_lb_res_cm_en_if_rw - lane 1 static termination driver middle point pull down enable
// RESET: 0x00
// [1] rx1_lb_res_dn_en_if_rw - lane 1 static termination driver test dn line enable
// RESET: 0x00
// [0] rx1_lb_res_dp_en_if_rw - lane 1 static termination driver test dp line enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_206() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x70dULL))) & vl::cat(vl::rd(rx1_pm_man_error_latch_rw), vl::rd(rx1_lb_prog_if_rw), vl::rd(rx1_lb_res_cm_en_if_rw), vl::rd(rx1_lb_res_dn_en_if_rw), vl::rd(rx1_lb_res_dp_en_if_rw));
    testdouts_0[205].write(v_.bit(0).sl());
    testdouts_1[205].write(v_.bit(1).sl());
    testdouts_2[205].write(v_.bit(2).sl());
    testdouts_3[205].write(v_.bit(3).sl());
    testdouts_4[205].write(v_.bit(4).sl());
    testdouts_5[205].write(v_.bit(5).sl());
    testdouts_6[205].write(v_.bit(6).sl());
    testdouts_7[205].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_0 - Lane 1 observability
// OFFSET: 0x72B
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rx1_rxhs_deserial_data_en_if - HS-RX deserializer reset observation for lane 1
// RESET: 0x00
// [2] hsrx_en_if_lane1 - HS-RX enable of lane 1
// RESET: 0x00
// [1] enable_lane1 - Signal that passes AFE control from soft macro to lane 1
// RESET: 0x00
// [0] bintpon_if_lane1 - Level shifter enable of lane 1
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_207() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x72bULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx1_rxhs_deserial_data_en_if), vl::rd(hsrx_en_if_lane1), vl::rd(enable_lane1), vl::rd(bintpon_if_lane1));
    testdouts_0[206].write(v_.bit(0).sl());
    testdouts_1[206].write(v_.bit(1).sl());
    testdouts_2[206].write(v_.bit(2).sl());
    testdouts_3[206].write(v_.bit(3).sl());
    testdouts_4[206].write(v_.bit(4).sl());
    testdouts_5[206].write(v_.bit(5).sl());
    testdouts_6[206].write(v_.bit(6).sl());
    testdouts_7[206].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_2 - Lane 1 observability
// OFFSET: 0x72D
// ATTR: READ, CUST
// FIELDS:
// [7] rx1_rxhs_rx_enterm_if - RX HS enable term
// RESET: 0x00
// [6] rx1_rxhs_rx_enlogic_if - RX HS enlogic
// RESET: 0x00
// [5] rx1_rxhs_rx_en_if - RX HS enable
// RESET: 0x00
// [4:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_208() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x72dULL))) & vl::cat(vl::rd(rx1_rxhs_rx_enterm_if), vl::rd(rx1_rxhs_rx_enlogic_if), vl::rd(rx1_rxhs_rx_en_if), vl::logic<5>(0x0ULL));
    testdouts_0[207].write(v_.bit(0).sl());
    testdouts_1[207].write(v_.bit(1).sl());
    testdouts_2[207].write(v_.bit(2).sl());
    testdouts_3[207].write(v_.bit(3).sl());
    testdouts_4[207].write(v_.bit(4).sl());
    testdouts_5[207].write(v_.bit(5).sl());
    testdouts_6[207].write(v_.bit(6).sl());
    testdouts_7[207].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_3 - Lane 1 observability
// OFFSET: 0x72E
// ATTR: READ, CUST
// FIELDS:
// [7:6] lprxdoutlp_lane1__1__0__ - LP-RX receiver outputs
// RESET: 0x00
// [5:4] lprxdoutcd_lane1__1__0__ - LP-CD receiver outputs
// RESET: 0x00
// [3] rx1_rxlp_ponulp_if - LP-RX ULP receiver power on
// RESET: 0x00
// [2] rx1_rxlp_ponlp_if - LP-RX LP receiver power on
// RESET: 0x00
// [1] rx1_rxlp_poncd_if - LP-RX contention detector power on
// RESET: 0x00
// [0] rx1_rxhs_rx_pon_if - RX HS power on
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_209() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x72eULL))) & vl::cat(vl::rd(lprxdoutlp_lane1), vl::rd(lprxdoutcd_lane1), vl::rd(rx1_rxlp_ponulp_if), vl::rd(rx1_rxlp_ponlp_if), vl::rd(rx1_rxlp_poncd_if), vl::rd(rx1_rxhs_rx_pon_if));
    testdouts_0[208].write(v_.bit(0).sl());
    testdouts_1[208].write(v_.bit(1).sl());
    testdouts_2[208].write(v_.bit(2).sl());
    testdouts_3[208].write(v_.bit(3).sl());
    testdouts_4[208].write(v_.bit(4).sl());
    testdouts_5[208].write(v_.bit(5).sl());
    testdouts_6[208].write(v_.bit(6).sl());
    testdouts_7[208].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_4 - Lane 1 observability
// OFFSET: 0x72F
// ATTR: READ, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx1_deskewcalfailed_if - Deskew calibration failed signal
// RESET: 0x00
// [3] rx1_deskewcaldone_if - Deskew calibration don
// RESET: 0x00
// [2] lsrxclk_lane1 - Low speed clock (XOR'ed from data lines)
// RESET: 0x00
// [1:0] lprxdoutulp_lane1__1__0__ - LP-RX ULP receiver outputs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_210() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x72fULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx1_deskewcalfailed_if), vl::rd(rx1_deskewcaldone_if), vl::rd(lsrxclk_lane1), vl::rd(lprxdoutulp_lane1));
    testdouts_0[209].write(v_.bit(0).sl());
    testdouts_1[209].write(v_.bit(1).sl());
    testdouts_2[209].write(v_.bit(2).sl());
    testdouts_3[209].write(v_.bit(3).sl());
    testdouts_4[209].write(v_.bit(4).sl());
    testdouts_5[209].write(v_.bit(5).sl());
    testdouts_6[209].write(v_.bit(6).sl());
    testdouts_7[209].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_5 - Lane 1 observability
// OFFSET: 0x730
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx1_rxhs_data_if__7__0__ - RX HS data
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_211() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x730ULL))) & vl::rd(rx1_rxhs_data_if);
    testdouts_0[210].write(v_.bit(0).sl());
    testdouts_1[210].write(v_.bit(1).sl());
    testdouts_2[210].write(v_.bit(2).sl());
    testdouts_3[210].write(v_.bit(3).sl());
    testdouts_4[210].write(v_.bit(4).sl());
    testdouts_5[210].write(v_.bit(5).sl());
    testdouts_6[210].write(v_.bit(6).sl());
    testdouts_7[210].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_6 - Lane 1 observability
// OFFSET: 0x731
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx1_pm_error_pm - Observation of the error flag generated by the master
// RESET: 0x00
// [4:3] reserved_1 - Reserved field
// RESET: 0x00
// [2] rx1_rxhs_word_clk_if - RX HS word clock
// RESET: 0x00
// [1] rx1_rxhs_feed_int_clk_if - RX HS internal clock feedback
// RESET: 0x00
// [0] rx1_rxhs_des_en_if - RX HS deserializer enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_212() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x731ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx1_pm_error_pm), vl::logic<2>(0x0ULL), vl::rd(rx1_rxhs_word_clk_if), vl::rd(rx1_rxhs_feed_int_clk_if), vl::rd(rx1_rxhs_des_en_if));
    testdouts_0[211].write(v_.bit(0).sl());
    testdouts_1[211].write(v_.bit(1).sl());
    testdouts_2[211].write(v_.bit(2).sl());
    testdouts_3[211].write(v_.bit(3).sl());
    testdouts_4[211].write(v_.bit(4).sl());
    testdouts_5[211].write(v_.bit(5).sl());
    testdouts_6[211].write(v_.bit(6).sl());
    testdouts_7[211].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_7 - Lane 1 observability
// OFFSET: 0x732
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx1_pm_error_counter__7__0__ - Error counter value
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_213() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x732ULL))) & vl::rd(rx1_pm_error_counter).slice<8>(0);
    testdouts_0[212].write(v_.bit(0).sl());
    testdouts_1[212].write(v_.bit(1).sl());
    testdouts_2[212].write(v_.bit(2).sl());
    testdouts_3[212].write(v_.bit(3).sl());
    testdouts_4[212].write(v_.bit(4).sl());
    testdouts_5[212].write(v_.bit(5).sl());
    testdouts_6[212].write(v_.bit(6).sl());
    testdouts_7[212].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_8 - Lane 1 observability
// OFFSET: 0x733
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx1_pm_error_counter__15__8__ - Error counter value
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_214() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x733ULL))) & vl::rd(rx1_pm_error_counter).slice<8>(8);
    testdouts_0[213].write(v_.bit(0).sl());
    testdouts_1[213].write(v_.bit(1).sl());
    testdouts_2[213].write(v_.bit(2).sl());
    testdouts_3[213].write(v_.bit(3).sl());
    testdouts_4[213].write(v_.bit(4).sl());
    testdouts_5[213].write(v_.bit(5).sl());
    testdouts_6[213].write(v_.bit(6).sl());
    testdouts_7[213].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_9 - Lane 1 observability
// OFFSET: 0x734
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:2] rx1_rxhs_ddl_phase_clk_if__4__0__ - Clock DDL setting chosen
// RESET: 0x00
// [1] rx1_rxhs_ddl_phase_change_p_if - DDL setting change latch pulse
// RESET: 0x00
// [0] rx1_pm_start - Pattern matcher start flag observation
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_215() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x734ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx1_rxhs_ddl_phase_clk_if), vl::rd(rx1_rxhs_ddl_phase_change_p_if), vl::rd(rx1_pm_start));
    testdouts_0[214].write(v_.bit(0).sl());
    testdouts_1[214].write(v_.bit(1).sl());
    testdouts_2[214].write(v_.bit(2).sl());
    testdouts_3[214].write(v_.bit(3).sl());
    testdouts_4[214].write(v_.bit(4).sl());
    testdouts_5[214].write(v_.bit(5).sl());
    testdouts_6[214].write(v_.bit(6).sl());
    testdouts_7[214].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_LANE_10 - Lane 1 observability
// OFFSET: 0x735
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx1_rxhs_ddl_phase_data_if__3__0__ - Data DDl setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_216() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x735ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx1_rxhs_ddl_phase_data_if));
    testdouts_0[215].write(v_.bit(0).sl());
    testdouts_1[215].write(v_.bit(1).sl());
    testdouts_2[215].write(v_.bit(2).sl());
    testdouts_3[215].write(v_.bit(3).sl());
    testdouts_4[215].write(v_.bit(4).sl());
    testdouts_5[215].write(v_.bit(5).sl());
    testdouts_6[215].write(v_.bit(6).sl());
    testdouts_7[215].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_OFFSET_CAL_0 - Lane 1 offset compensation control
// OFFSET: 0x77B
// ATTR: RW, CUST
// FIELDS:
// [7] rx1_rxhs_data_offset_cal_en_ovr_en_rw - Data offset calibration override enable control
// RESET: 0x00
// [6] rx1_cal_done_ovr_rw - Calibration done override enable control
// RESET: 0x00
// [5] rx1_cal_done_ovr_en_rw - Calibration done override enable control
// RESET: 0x00
// [4] rx1_start_calibration_ovr_rw - RX start calibration
// RESET: 0x00
// [3] rx1_start_calibration_ovr_en_rw - RX start calibration
// RESET: 0x00
// [2:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_217() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x77bULL))) & vl::cat(vl::rd(rx1_rxhs_data_offset_cal_en_ovr_en_rw), vl::rd(rx1_cal_done_ovr_rw), vl::rd(rx1_cal_done_ovr_en_rw), vl::rd(rx1_start_calibration_ovr_rw), vl::rd(rx1_start_calibration_ovr_en_rw), vl::logic<3>(0x0ULL));
    testdouts_0[216].write(v_.bit(0).sl());
    testdouts_1[216].write(v_.bit(1).sl());
    testdouts_2[216].write(v_.bit(2).sl());
    testdouts_3[216].write(v_.bit(3).sl());
    testdouts_4[216].write(v_.bit(4).sl());
    testdouts_5[216].write(v_.bit(5).sl());
    testdouts_6[216].write(v_.bit(6).sl());
    testdouts_7[216].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_OFFSET_CAL_1 - Lane 1 offset compensation control
// OFFSET: 0x77C
// ATTR: RW, CUST
// FIELDS:
// [7] rx1_rxhs_data_offset_cal_ovr_en_rw - Offset calibration setting override
// RESET: 0x00
// [6:1] rx1_rxhs_data_offset_cal_ovr_rw__5__0__ - Offset calibration setting override enable
// RESET: 0x00
// [0] rx1_rxhs_data_offset_cal_en_ovr_rw - Data offset calibration override enable control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_218() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x77cULL))) & vl::cat(vl::rd(rx1_rxhs_data_offset_cal_ovr_en_rw), vl::rd(rx1_rxhs_data_offset_cal_ovr_rw), vl::rd(rx1_rxhs_data_offset_cal_en_ovr_rw));
    testdouts_0[217].write(v_.bit(0).sl());
    testdouts_1[217].write(v_.bit(1).sl());
    testdouts_2[217].write(v_.bit(2).sl());
    testdouts_3[217].write(v_.bit(3).sl());
    testdouts_4[217].write(v_.bit(4).sl());
    testdouts_5[217].write(v_.bit(5).sl());
    testdouts_6[217].write(v_.bit(6).sl());
    testdouts_7[217].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_OFFSET_CAL_2 - Lane 1 offset compensation control
// OFFSET: 0x77D
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx1_rxhs_data_ovr_rw__7__0__ - RX HS data override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_219() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x77dULL))) & vl::rd(rx1_rxhs_data_ovr_rw);
    testdouts_0[218].write(v_.bit(0).sl());
    testdouts_1[218].write(v_.bit(1).sl());
    testdouts_2[218].write(v_.bit(2).sl());
    testdouts_3[218].write(v_.bit(3).sl());
    testdouts_4[218].write(v_.bit(4).sl());
    testdouts_5[218].write(v_.bit(5).sl());
    testdouts_6[218].write(v_.bit(6).sl());
    testdouts_7[218].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_OFFSET_CAL_3 - Lane 1 offset compensation control
// OFFSET: 0x77E
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] rx1_rxhs_repeatcal_rw - RX HS repeat calibration
// RESET: 0x00
// [0] rx1_rxhs_data_ovr_en_rw - RX HS data override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_220() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x77eULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(rx1_rxhs_repeatcal_rw), vl::rd(rx1_rxhs_data_ovr_en_rw));
    testdouts_0[219].write(v_.bit(0).sl());
    testdouts_1[219].write(v_.bit(1).sl());
    testdouts_2[219].write(v_.bit(2).sl());
    testdouts_3[219].write(v_.bit(3).sl());
    testdouts_4[219].write(v_.bit(4).sl());
    testdouts_5[219].write(v_.bit(5).sl());
    testdouts_6[219].write(v_.bit(6).sl());
    testdouts_7[219].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_OFFSET_CAL_0 - Lane 1 offset compensation observability
// OFFSET: 0x79F
// ATTR: READ, CUST
// FIELDS:
// [7:3] reserved_0 - Reserved field
// RESET: 0x00
// [2] rx1_cal_error - Offset calibration algorithm error
// RESET: 0x00
// [1] rx1_cal_done - Offset calibration algorithm done
// RESET: 0x00
// [0] rx1_start_calibration - Offset calibration algorithm start
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_221() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x79fULL))) & vl::cat(vl::logic<5>(0x0ULL), vl::rd(rx1_cal_error), vl::rd(rx1_cal_done), vl::rd(rx1_start_calibration));
    testdouts_0[220].write(v_.bit(0).sl());
    testdouts_1[220].write(v_.bit(1).sl());
    testdouts_2[220].write(v_.bit(2).sl());
    testdouts_3[220].write(v_.bit(3).sl());
    testdouts_4[220].write(v_.bit(4).sl());
    testdouts_5[220].write(v_.bit(5).sl());
    testdouts_6[220].write(v_.bit(6).sl());
    testdouts_7[220].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_OFFSET_CAL_1 - Lane 1 offset compensation observability
// OFFSET: 0x7A0
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx1_rxhs_data_int__7__0__ - HS-RX data received (AFE output)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_222() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7a0ULL))) & vl::rd(rx1_rxhs_data_int);
    testdouts_0[221].write(v_.bit(0).sl());
    testdouts_1[221].write(v_.bit(1).sl());
    testdouts_2[221].write(v_.bit(2).sl());
    testdouts_3[221].write(v_.bit(3).sl());
    testdouts_4[221].write(v_.bit(4).sl());
    testdouts_5[221].write(v_.bit(5).sl());
    testdouts_6[221].write(v_.bit(6).sl());
    testdouts_7[221].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_OFFSET_CAL_2 - Lane 1 offset compensation observability
// OFFSET: 0x7A1
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] rx1_rxhs_data_offset_cal_en - Offset calibration algorithm enable
// RESET: 0x00
// [5:0] rx1_rxhs_data_offset_cal__5__0__ - Offset calibration algorithm setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_223() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7a1ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx1_rxhs_data_offset_cal_en), vl::rd(rx1_rxhs_data_offset_cal));
    testdouts_0[222].write(v_.bit(0).sl());
    testdouts_1[222].write(v_.bit(1).sl());
    testdouts_2[222].write(v_.bit(2).sl());
    testdouts_3[222].write(v_.bit(3).sl());
    testdouts_4[222].write(v_.bit(4).sl());
    testdouts_5[222].write(v_.bit(5).sl());
    testdouts_6[222].write(v_.bit(6).sl());
    testdouts_7[222].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_OFFSET_CAL_3 - Lane 1 offset compensation observability
// OFFSET: 0x7A2
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx1_total_ones__7__0__ - Offset calibration algorithm counter of 1s
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_224() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7a2ULL))) & vl::rd(rx1_total_ones);
    testdouts_0[223].write(v_.bit(0).sl());
    testdouts_1[223].write(v_.bit(1).sl());
    testdouts_2[223].write(v_.bit(2).sl());
    testdouts_3[223].write(v_.bit(3).sl());
    testdouts_4[223].write(v_.bit(4).sl());
    testdouts_5[223].write(v_.bit(5).sl());
    testdouts_6[223].write(v_.bit(6).sl());
    testdouts_7[223].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_DDL_0 - Lane 1 DDL tune observability
// OFFSET: 0x7E0
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rx1_ddl_start_calibration_if - DDL tune digital algorithm start
// RESET: 0x00
// [2] rx1_ddl_finished_if - DDL tune done indication
// RESET: 0x00
// [1] rx1_ddl_error_flag_if - DDL tune error
// RESET: 0x00
// [0] ddl_tune_en_lane1_if - DDL tune analog circuitry enable indication
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_225() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7e0ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx1_ddl_start_calibration_if), vl::rd(rx1_ddl_finished_if), vl::rd(rx1_ddl_error_flag_if), vl::rd(ddl_tune_en_lane1_if));
    testdouts_0[224].write(v_.bit(0).sl());
    testdouts_1[224].write(v_.bit(1).sl());
    testdouts_2[224].write(v_.bit(2).sl());
    testdouts_3[224].write(v_.bit(3).sl());
    testdouts_4[224].write(v_.bit(4).sl());
    testdouts_5[224].write(v_.bit(5).sl());
    testdouts_6[224].write(v_.bit(6).sl());
    testdouts_7[224].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_DDL_1 - Lane 1 DDL tune observability
// OFFSET: 0x7E1
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx1_freq_meas_out_if__7__0__ - RX DDL tunning oscillation measurement
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_226() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7e1ULL))) & vl::rd(rx1_freq_meas_out_if).slice<8>(0);
    testdouts_0[225].write(v_.bit(0).sl());
    testdouts_1[225].write(v_.bit(1).sl());
    testdouts_2[225].write(v_.bit(2).sl());
    testdouts_3[225].write(v_.bit(3).sl());
    testdouts_4[225].write(v_.bit(4).sl());
    testdouts_5[225].write(v_.bit(5).sl());
    testdouts_6[225].write(v_.bit(6).sl());
    testdouts_7[225].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_DDL_2 - Lane 1 DDL tune observability
// OFFSET: 0x7E2
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx1_freq_meas_out_if__11__8__ - RX DDL tunning oscillation measurement
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_227() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7e2ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx1_freq_meas_out_if).slice<4>(8));
    testdouts_0[226].write(v_.bit(0).sl());
    testdouts_1[226].write(v_.bit(1).sl());
    testdouts_2[226].write(v_.bit(2).sl());
    testdouts_3[226].write(v_.bit(3).sl());
    testdouts_4[226].write(v_.bit(4).sl());
    testdouts_5[226].write(v_.bit(5).sl());
    testdouts_6[226].write(v_.bit(6).sl());
    testdouts_7[226].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_DDL_3 - Lane 1 DDL tune observability
// OFFSET: 0x7E3
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx1_osc_freq_target_if__7__0__ - RX oscillator frequency target obs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_228() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7e3ULL))) & vl::rd(rx1_osc_freq_target_if).slice<8>(0);
    testdouts_0[227].write(v_.bit(0).sl());
    testdouts_1[227].write(v_.bit(1).sl());
    testdouts_2[227].write(v_.bit(2).sl());
    testdouts_3[227].write(v_.bit(3).sl());
    testdouts_4[227].write(v_.bit(4).sl());
    testdouts_5[227].write(v_.bit(5).sl());
    testdouts_6[227].write(v_.bit(6).sl());
    testdouts_7[227].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_DDL_4 - Lane 1 DDL tune observability
// OFFSET: 0x7E4
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx1_osc_freq_target_if__11__8__ - RX oscillator frequency target obs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_229() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7e4ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx1_osc_freq_target_if).slice<4>(8));
    testdouts_0[228].write(v_.bit(0).sl());
    testdouts_1[228].write(v_.bit(1).sl());
    testdouts_2[228].write(v_.bit(2).sl());
    testdouts_3[228].write(v_.bit(3).sl());
    testdouts_4[228].write(v_.bit(4).sl());
    testdouts_5[228].write(v_.bit(5).sl());
    testdouts_6[228].write(v_.bit(6).sl());
    testdouts_7[228].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE1_DDL_5 - Lane 1 DDL tune observability
// OFFSET: 0x7E5
// ATTR: READ, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4:0] rx1_rxhs_ddl_tune_if__4__0__ - RX HS DDL tunning setting
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_230() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x7e5ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::logic<1>(0x0ULL), vl::rd(rx1_rxhs_ddl_tune_if));
    testdouts_0[229].write(v_.bit(0).sl());
    testdouts_1[229].write(v_.bit(1).sl());
    testdouts_2[229].write(v_.bit(2).sl());
    testdouts_3[229].write(v_.bit(3).sl());
    testdouts_4[229].write(v_.bit(4).sl());
    testdouts_5[229].write(v_.bit(5).sl());
    testdouts_6[229].write(v_.bit(6).sl());
    testdouts_7[229].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_DDL_0 - Lane 1 DDL tune control
// OFFSET: 0x806
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx1_rxhs_ddl_tune_en_if_rw - RX HS DDL tune enable
// RESET: 0x00
// [4] rx1_rxhs_ddl_tune_en_bypass_rw - RX HS DDL tune enable bypass
// RESET: 0x00
// [3] rx1_ddl_start_calibration_ovr_rw - RX DDL tunning calibration start override
// RESET: 0x00
// [2] rx1_ddl_start_calibration_ovr_en_rw - RX DDL tunning calibration start override enable
// RESET: 0x00
// [1] rx1_ddl_finished_ovr_rw - RX DDL tunning done override
// RESET: 0x00
// [0] rx1_ddl_finished_ovr_en_rw - RX DDL tunning done override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_231() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x806ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx1_rxhs_ddl_tune_en_if_rw), vl::rd(rx1_rxhs_ddl_tune_en_bypass_rw), vl::rd(rx1_ddl_start_calibration_ovr_rw), vl::rd(rx1_ddl_start_calibration_ovr_en_rw), vl::rd(rx1_ddl_finished_ovr_rw), vl::rd(rx1_ddl_finished_ovr_en_rw));
    testdouts_0[230].write(v_.bit(0).sl());
    testdouts_1[230].write(v_.bit(1).sl());
    testdouts_2[230].write(v_.bit(2).sl());
    testdouts_3[230].write(v_.bit(3).sl());
    testdouts_4[230].write(v_.bit(4).sl());
    testdouts_5[230].write(v_.bit(5).sl());
    testdouts_6[230].write(v_.bit(6).sl());
    testdouts_7[230].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_DDL_1 - Lane 1 DDL tune control
// OFFSET: 0x807
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx1_rxhs_ddl_tune_ovr_en_rw - RX HS DDL tune override enable control
// RESET: 0x00
// [4:0] rx1_rxhs_ddl_tune_ovr_rw__4__0__ - RX HS DDL tune override for lane 1
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_232() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x807ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx1_rxhs_ddl_tune_ovr_en_rw), vl::rd(rx1_rxhs_ddl_tune_ovr_rw));
    testdouts_0[231].write(v_.bit(0).sl());
    testdouts_1[231].write(v_.bit(1).sl());
    testdouts_2[231].write(v_.bit(2).sl());
    testdouts_3[231].write(v_.bit(3).sl());
    testdouts_4[231].write(v_.bit(4).sl());
    testdouts_5[231].write(v_.bit(5).sl());
    testdouts_6[231].write(v_.bit(6).sl());
    testdouts_7[231].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_DDL_4 - Lane 1 DDL tune control
// OFFSET: 0x80A
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx1_osc_freq_target_if_ovr_rw__7__0__ - Oscillating frequency override Target
// RESET: 0x77
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_233() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x80aULL))) & vl::rd(rx1_osc_freq_target_if_ovr_rw).slice<8>(0);
    testdouts_0[232].write(v_.bit(0).sl());
    testdouts_1[232].write(v_.bit(1).sl());
    testdouts_2[232].write(v_.bit(2).sl());
    testdouts_3[232].write(v_.bit(3).sl());
    testdouts_4[232].write(v_.bit(4).sl());
    testdouts_5[232].write(v_.bit(5).sl());
    testdouts_6[232].write(v_.bit(6).sl());
    testdouts_7[232].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_DDL_5 - Lane 1 DDL tune control
// OFFSET: 0x80B
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx1_osc_freq_target_if_ovr_rw__11__8__ - Oscillating frequency override Target
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_234() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x80bULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx1_osc_freq_target_if_ovr_rw).slice<4>(8));
    testdouts_0[233].write(v_.bit(0).sl());
    testdouts_1[233].write(v_.bit(1).sl());
    testdouts_2[233].write(v_.bit(2).sl());
    testdouts_3[233].write(v_.bit(3).sl());
    testdouts_4[233].write(v_.bit(4).sl());
    testdouts_5[233].write(v_.bit(5).sl());
    testdouts_6[233].write(v_.bit(6).sl());
    testdouts_7[233].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE1_DDL_6 - Lane 1 DDL tune control
// OFFSET: 0x80C
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] rx1_osc_freq_target_if_ovr_en_rw - Override for oscillation frequency target
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_235() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x80cULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(rx1_osc_freq_target_if_ovr_en_rw));
    testdouts_0[234].write(v_.bit(0).sl());
    testdouts_1[234].write(v_.bit(1).sl());
    testdouts_2[234].write(v_.bit(2).sl());
    testdouts_3[234].write(v_.bit(3).sl());
    testdouts_4[234].write(v_.bit(4).sl());
    testdouts_5[234].write(v_.bit(5).sl());
    testdouts_6[234].write(v_.bit(6).sl());
    testdouts_7[234].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_0 - Lane 2 control
// OFFSET: 0x901
// ATTR: RW, CUST
// FIELDS:
// [7] hsrx_en_bypass_lane2_rw - HS RX enable bypass
// RESET: 0x00
// [6] bintpon_lane2_rw - Bias block power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [5] bintpon_bypass_lane2_rw - Bias block power-on bypass override
// RESET: 0x00
// [4:1] atb_sel_lane2_rw__3__0__ - Analog Test Bus signals selection:
// RESET: 0x00
// [0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_236() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x901ULL))) & vl::cat(vl::rd(hsrx_en_bypass_lane2_rw), vl::rd(bintpon_lane2_rw), vl::rd(bintpon_bypass_lane2_rw), vl::rd(atb_sel_lane2_rw), vl::logic<1>(0x0ULL));
    testdouts_0[235].write(v_.bit(0).sl());
    testdouts_1[235].write(v_.bit(1).sl());
    testdouts_2[235].write(v_.bit(2).sl());
    testdouts_3[235].write(v_.bit(3).sl());
    testdouts_4[235].write(v_.bit(4).sl());
    testdouts_5[235].write(v_.bit(5).sl());
    testdouts_6[235].write(v_.bit(6).sl());
    testdouts_7[235].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_1 - Lane 2 control
// OFFSET: 0x902
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] hsrxpon_if_lane2_rw - Power on of the RX HS
// RESET: 0x00
// [3] hsrxpon_bypass_lane2_rw - Power on of the RX HS bypass control
// RESET: 0x00
// [2] hsrxenterm_if_lane2_rw - RX HS enable term
// RESET: 0x00
// [1] hsrxenterm_bypass_lane2_rw - Bypass control of RX high speed enable term
// RESET: 0x00
// [0] hsrx_en_if_lane2_tester_rw - HS RX enable for tester
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_237() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x902ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(hsrxpon_if_lane2_rw), vl::rd(hsrxpon_bypass_lane2_rw), vl::rd(hsrxenterm_if_lane2_rw), vl::rd(hsrxenterm_bypass_lane2_rw), vl::rd(hsrx_en_if_lane2_tester_rw));
    testdouts_0[236].write(v_.bit(0).sl());
    testdouts_1[236].write(v_.bit(1).sl());
    testdouts_2[236].write(v_.bit(2).sl());
    testdouts_3[236].write(v_.bit(3).sl());
    testdouts_4[236].write(v_.bit(4).sl());
    testdouts_5[236].write(v_.bit(5).sl());
    testdouts_6[236].write(v_.bit(6).sl());
    testdouts_7[236].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_3 - Lane 2 control
// OFFSET: 0x904
// ATTR: RW, CUST
// FIELDS:
// [7] lprxponcd_bypass_lane2_rw - LP RX Contention Detector power-on bypass override
// RESET: 0x00
// [6:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_238() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x904ULL))) & vl::cat(vl::rd(lprxponcd_bypass_lane2_rw), vl::logic<7>(0x0ULL));
    testdouts_0[237].write(v_.bit(0).sl());
    testdouts_1[237].write(v_.bit(1).sl());
    testdouts_2[237].write(v_.bit(2).sl());
    testdouts_3[237].write(v_.bit(3).sl());
    testdouts_4[237].write(v_.bit(4).sl());
    testdouts_5[237].write(v_.bit(5).sl());
    testdouts_6[237].write(v_.bit(6).sl());
    testdouts_7[237].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_4 - Lane 2 control
// OFFSET: 0x905
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] lprxponulp_lane2_rw - LP RX ULP power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [3] lprxponulp_bypass_lane2_rw - LP RX ULP power-on bypass override
// RESET: 0x00
// [2] lprxponlp_lane2_rw - LP RX LP power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [1] lprxponlp_bypass_lane2_rw - LP RX LP power-on bypass override
// RESET: 0x00
// [0] lprxponcd_lane2_rw - LP RX Contention Detector power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_239() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x905ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(lprxponulp_lane2_rw), vl::rd(lprxponulp_bypass_lane2_rw), vl::rd(lprxponlp_lane2_rw), vl::rd(lprxponlp_bypass_lane2_rw), vl::rd(lprxponcd_lane2_rw));
    testdouts_0[238].write(v_.bit(0).sl());
    testdouts_1[238].write(v_.bit(1).sl());
    testdouts_2[238].write(v_.bit(2).sl());
    testdouts_3[238].write(v_.bit(3).sl());
    testdouts_4[238].write(v_.bit(4).sl());
    testdouts_5[238].write(v_.bit(5).sl());
    testdouts_6[238].write(v_.bit(6).sl());
    testdouts_7[238].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_6 - Lane 2 control
// OFFSET: 0x907
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx2_rxhs_feed_int_clk_ovr_en_rw - HS-RX source clock override enable control for lane 2. Selects between internal clock source or external (receiver)
// RESET: 0x00
// [4] rx2_rxhs_feed_int_clk_ovr_rw - HS-RX source clock override control for lane 2. Selects between internal clock source or external (receiver)
// RESET: 0x00
// [3:1] reserved_1 - Reserved field
// RESET: 0x00
// [0] polarity_lane2_rw -
// NOTES: Polarity change control
// <br>0 - Two PHYs with P/N ports connected to P/N respectively
// <br>1 - Two PHYs with P/N ports connected to N/P respectively
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_240() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x907ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx2_rxhs_feed_int_clk_ovr_en_rw), vl::rd(rx2_rxhs_feed_int_clk_ovr_rw), vl::logic<3>(0x0ULL), vl::rd(polarity_lane2_rw));
    testdouts_0[239].write(v_.bit(0).sl());
    testdouts_1[239].write(v_.bit(1).sl());
    testdouts_2[239].write(v_.bit(2).sl());
    testdouts_3[239].write(v_.bit(3).sl());
    testdouts_4[239].write(v_.bit(4).sl());
    testdouts_5[239].write(v_.bit(5).sl());
    testdouts_6[239].write(v_.bit(6).sl());
    testdouts_7[239].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_7 - Lane 2 control
// OFFSET: 0x908
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx2_rxhs_clk_from_long_channel_if_rw - RX HS clock from long channel
// RESET: 0x00
// [4] recal_lane2_ovr_rw - Override Enable voltage control for recal
// RESET: 0x00
// [3] recal_lane2_ovr_en_rw - Override Enable voltage control for recal
// RESET: 0x00
// [2:0] reserved_1 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_241() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x908ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx2_rxhs_clk_from_long_channel_if_rw), vl::rd(recal_lane2_ovr_rw), vl::rd(recal_lane2_ovr_en_rw), vl::logic<3>(0x0ULL));
    testdouts_0[240].write(v_.bit(0).sl());
    testdouts_1[240].write(v_.bit(1).sl());
    testdouts_2[240].write(v_.bit(2).sl());
    testdouts_3[240].write(v_.bit(3).sl());
    testdouts_4[240].write(v_.bit(4).sl());
    testdouts_5[240].write(v_.bit(5).sl());
    testdouts_6[240].write(v_.bit(6).sl());
    testdouts_7[240].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_8 - Lane 2 control
// OFFSET: 0x909
// ATTR: RW, CUST
// FIELDS:
// [7] rx_des_en_lane2_rw - RX deserializer enable
// RESET: 0x00
// [6] rx_des_en_bypass_lane2_rw - RX deserializer enable bypass control
// RESET: 0x00
// [5:3] reserved_0 - Reserved field
// RESET: 0x00
// [2:0] rx2_rxhs_data_eq_if_rw__2__0__ - RX HS data equalizer
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_242() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x909ULL))) & vl::cat(vl::rd(rx_des_en_lane2_rw), vl::rd(rx_des_en_bypass_lane2_rw), vl::logic<3>(0x0ULL), vl::rd(rx2_rxhs_data_eq_if_rw));
    testdouts_0[241].write(v_.bit(0).sl());
    testdouts_1[241].write(v_.bit(1).sl());
    testdouts_2[241].write(v_.bit(2).sl());
    testdouts_3[241].write(v_.bit(3).sl());
    testdouts_4[241].write(v_.bit(4).sl());
    testdouts_5[241].write(v_.bit(5).sl());
    testdouts_6[241].write(v_.bit(6).sl());
    testdouts_7[241].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_9 - Lane 2 control
// OFFSET: 0x90A
// ATTR: RW, CUST
// FIELDS:
// [7] rx2_pm_sample_error_counter_rw - strobe to read the error counter
// RESET: 0x00
// [6] rx2_pm_start_control_ovr_en_rw - Pattern matcher start override enable.
// RESET: 0x00
// [5] rx2_pm_start_control_ovr_rw - Pattern matcher start override.
// RESET: 0x00
// [4:1] rx2_pm_mode_r_rw__3__0__ - Configuration mode for the pattern matcher
// RESET: 0x07
// [0] rx2_data_retiming_out_swap_sel_rw - retiming mux control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_243() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x90aULL))) & vl::cat(vl::rd(rx2_pm_sample_error_counter_rw), vl::rd(rx2_pm_start_control_ovr_en_rw), vl::rd(rx2_pm_start_control_ovr_rw), vl::rd(rx2_pm_mode_r_rw), vl::rd(rx2_data_retiming_out_swap_sel_rw));
    testdouts_0[242].write(v_.bit(0).sl());
    testdouts_1[242].write(v_.bit(1).sl());
    testdouts_2[242].write(v_.bit(2).sl());
    testdouts_3[242].write(v_.bit(3).sl());
    testdouts_4[242].write(v_.bit(4).sl());
    testdouts_5[242].write(v_.bit(5).sl());
    testdouts_6[242].write(v_.bit(6).sl());
    testdouts_7[242].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_10 - Lane 2 control
// OFFSET: 0x90B
// ATTR: RW, CUST
// FIELDS:
// [7] rx2_rxhs_ddl_phase_clk_ovr_en_rw - RX HS DDL phase data override control
// RESET: 0x00
// [6:2] rx2_rxhs_ddl_phase_clk_ovr_rw__4__0__ - RX HS DDL phase clock override
// RESET: 0x00
// [1] rx2_rxhs_ddl_phase_change_p_ovr_en_rw - RX HS DDL phase changed override control
// RESET: 0x00
// [0] rx2_rxhs_ddl_phase_change_p_ovr_rw - RX HS DDL phase changed override control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_244() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x90bULL))) & vl::cat(vl::rd(rx2_rxhs_ddl_phase_clk_ovr_en_rw), vl::rd(rx2_rxhs_ddl_phase_clk_ovr_rw), vl::rd(rx2_rxhs_ddl_phase_change_p_ovr_en_rw), vl::rd(rx2_rxhs_ddl_phase_change_p_ovr_rw));
    testdouts_0[243].write(v_.bit(0).sl());
    testdouts_1[243].write(v_.bit(1).sl());
    testdouts_2[243].write(v_.bit(2).sl());
    testdouts_3[243].write(v_.bit(3).sl());
    testdouts_4[243].write(v_.bit(4).sl());
    testdouts_5[243].write(v_.bit(5).sl());
    testdouts_6[243].write(v_.bit(6).sl());
    testdouts_7[243].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_11 - Lane 2 control
// OFFSET: 0x90C
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx2_rxhs_ddl_phase_data_ovr_en_rw - Override enable for ddl phase data
// RESET: 0x00
// [3:0] rx2_rxhs_ddl_phase_data_ovr_rw__3__0__ - RX HS DDL phase clock override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_245() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x90cULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx2_rxhs_ddl_phase_data_ovr_en_rw), vl::rd(rx2_rxhs_ddl_phase_data_ovr_rw));
    testdouts_0[244].write(v_.bit(0).sl());
    testdouts_1[244].write(v_.bit(1).sl());
    testdouts_2[244].write(v_.bit(2).sl());
    testdouts_3[244].write(v_.bit(3).sl());
    testdouts_4[244].write(v_.bit(4).sl());
    testdouts_5[244].write(v_.bit(5).sl());
    testdouts_6[244].write(v_.bit(6).sl());
    testdouts_7[244].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_LANE_12 - Lane 2 control
// OFFSET: 0x90D
// ATTR: RW, CUST
// FIELDS:
// [7] rx2_pm_man_error_latch_rw - Allows manual pattern matcher error counter latch. Latching is controlled by rx2_pm_sample_error_counter
// RESET: 0x00
// [6:3] rx2_lb_prog_if_rw__3__0__ - lane 2 receiver biasing currents
// RESET: 0x0F
// [2] rx2_lb_res_cm_en_if_rw - lane 2 static termination driver middle point pull down enable
// RESET: 0x00
// [1] rx2_lb_res_dn_en_if_rw - lane 2 static termination driver test dn line enable
// RESET: 0x00
// [0] rx2_lb_res_dp_en_if_rw - lane 2 static termination driver test dp line enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_246() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x90dULL))) & vl::cat(vl::rd(rx2_pm_man_error_latch_rw), vl::rd(rx2_lb_prog_if_rw), vl::rd(rx2_lb_res_cm_en_if_rw), vl::rd(rx2_lb_res_dn_en_if_rw), vl::rd(rx2_lb_res_dp_en_if_rw));
    testdouts_0[245].write(v_.bit(0).sl());
    testdouts_1[245].write(v_.bit(1).sl());
    testdouts_2[245].write(v_.bit(2).sl());
    testdouts_3[245].write(v_.bit(3).sl());
    testdouts_4[245].write(v_.bit(4).sl());
    testdouts_5[245].write(v_.bit(5).sl());
    testdouts_6[245].write(v_.bit(6).sl());
    testdouts_7[245].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_0 - Lane 2 observability
// OFFSET: 0x92B
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rx2_rxhs_deserial_data_en_if - HS-RX deserializer reset observation for lane 2
// RESET: 0x00
// [2] hsrx_en_if_lane2 - HS-RX enable of lane 2
// RESET: 0x00
// [1] enable_lane2 - Signal that passes AFE control from soft macro to lane 2
// RESET: 0x00
// [0] bintpon_if_lane2 - Level shifter enable of lane 2
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_247() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x92bULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx2_rxhs_deserial_data_en_if), vl::rd(hsrx_en_if_lane2), vl::rd(enable_lane2), vl::rd(bintpon_if_lane2));
    testdouts_0[246].write(v_.bit(0).sl());
    testdouts_1[246].write(v_.bit(1).sl());
    testdouts_2[246].write(v_.bit(2).sl());
    testdouts_3[246].write(v_.bit(3).sl());
    testdouts_4[246].write(v_.bit(4).sl());
    testdouts_5[246].write(v_.bit(5).sl());
    testdouts_6[246].write(v_.bit(6).sl());
    testdouts_7[246].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_2 - Lane 2 observability
// OFFSET: 0x92D
// ATTR: READ, CUST
// FIELDS:
// [7] rx2_rxhs_rx_enterm_if - RX HS enable term
// RESET: 0x00
// [6] rx2_rxhs_rx_enlogic_if - RX HS enlogic
// RESET: 0x00
// [5] rx2_rxhs_rx_en_if - RX HS enable
// RESET: 0x00
// [4:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_248() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x92dULL))) & vl::cat(vl::rd(rx2_rxhs_rx_enterm_if), vl::rd(rx2_rxhs_rx_enlogic_if), vl::rd(rx2_rxhs_rx_en_if), vl::logic<5>(0x0ULL));
    testdouts_0[247].write(v_.bit(0).sl());
    testdouts_1[247].write(v_.bit(1).sl());
    testdouts_2[247].write(v_.bit(2).sl());
    testdouts_3[247].write(v_.bit(3).sl());
    testdouts_4[247].write(v_.bit(4).sl());
    testdouts_5[247].write(v_.bit(5).sl());
    testdouts_6[247].write(v_.bit(6).sl());
    testdouts_7[247].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_3 - Lane 2 observability
// OFFSET: 0x92E
// ATTR: READ, CUST
// FIELDS:
// [7:6] lprxdoutlp_lane2__1__0__ - LP-RX receiver outputs
// RESET: 0x00
// [5:4] lprxdoutcd_lane2__1__0__ - LP-CD receiver outputs
// RESET: 0x00
// [3] rx2_rxlp_ponulp_if - LP-RX ULP receiver power on
// RESET: 0x00
// [2] rx2_rxlp_ponlp_if - LP-RX LP receiver power on
// RESET: 0x00
// [1] rx2_rxlp_poncd_if - LP-RX contention detector power on
// RESET: 0x00
// [0] rx2_rxhs_rx_pon_if - RX HS power on
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_249() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x92eULL))) & vl::cat(vl::rd(lprxdoutlp_lane2), vl::rd(lprxdoutcd_lane2), vl::rd(rx2_rxlp_ponulp_if), vl::rd(rx2_rxlp_ponlp_if), vl::rd(rx2_rxlp_poncd_if), vl::rd(rx2_rxhs_rx_pon_if));
    testdouts_0[248].write(v_.bit(0).sl());
    testdouts_1[248].write(v_.bit(1).sl());
    testdouts_2[248].write(v_.bit(2).sl());
    testdouts_3[248].write(v_.bit(3).sl());
    testdouts_4[248].write(v_.bit(4).sl());
    testdouts_5[248].write(v_.bit(5).sl());
    testdouts_6[248].write(v_.bit(6).sl());
    testdouts_7[248].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_4 - Lane 2 observability
// OFFSET: 0x92F
// ATTR: READ, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx2_deskewcalfailed_if - Deskew calibration failed signal
// RESET: 0x00
// [3] rx2_deskewcaldone_if - Deskew calibration don
// RESET: 0x00
// [2] lsrxclk_lane2 - Low speed clock (XOR'ed from data lines)
// RESET: 0x00
// [1:0] lprxdoutulp_lane2__1__0__ - LP-RX ULP receiver outputs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_250() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x92fULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx2_deskewcalfailed_if), vl::rd(rx2_deskewcaldone_if), vl::rd(lsrxclk_lane2), vl::rd(lprxdoutulp_lane2));
    testdouts_0[249].write(v_.bit(0).sl());
    testdouts_1[249].write(v_.bit(1).sl());
    testdouts_2[249].write(v_.bit(2).sl());
    testdouts_3[249].write(v_.bit(3).sl());
    testdouts_4[249].write(v_.bit(4).sl());
    testdouts_5[249].write(v_.bit(5).sl());
    testdouts_6[249].write(v_.bit(6).sl());
    testdouts_7[249].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_5 - Lane 2 observability
// OFFSET: 0x930
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx2_rxhs_data_if__7__0__ - RX HS data
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_251() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x930ULL))) & vl::rd(rx2_rxhs_data_if);
    testdouts_0[250].write(v_.bit(0).sl());
    testdouts_1[250].write(v_.bit(1).sl());
    testdouts_2[250].write(v_.bit(2).sl());
    testdouts_3[250].write(v_.bit(3).sl());
    testdouts_4[250].write(v_.bit(4).sl());
    testdouts_5[250].write(v_.bit(5).sl());
    testdouts_6[250].write(v_.bit(6).sl());
    testdouts_7[250].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_6 - Lane 2 observability
// OFFSET: 0x931
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx2_pm_error_pm - Observation of the error flag generated by the master
// RESET: 0x00
// [4:3] reserved_1 - Reserved field
// RESET: 0x00
// [2] rx2_rxhs_word_clk_if - RX HS word clock
// RESET: 0x00
// [1] rx2_rxhs_feed_int_clk_if - RX HS internal clock feedback
// RESET: 0x00
// [0] rx2_rxhs_des_en_if - RX HS deserializer enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_252() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x931ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx2_pm_error_pm), vl::logic<2>(0x0ULL), vl::rd(rx2_rxhs_word_clk_if), vl::rd(rx2_rxhs_feed_int_clk_if), vl::rd(rx2_rxhs_des_en_if));
    testdouts_0[251].write(v_.bit(0).sl());
    testdouts_1[251].write(v_.bit(1).sl());
    testdouts_2[251].write(v_.bit(2).sl());
    testdouts_3[251].write(v_.bit(3).sl());
    testdouts_4[251].write(v_.bit(4).sl());
    testdouts_5[251].write(v_.bit(5).sl());
    testdouts_6[251].write(v_.bit(6).sl());
    testdouts_7[251].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_7 - Lane 2 observability
// OFFSET: 0x932
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx2_pm_error_counter__7__0__ - Error counter value
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_253() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x932ULL))) & vl::rd(rx2_pm_error_counter).slice<8>(0);
    testdouts_0[252].write(v_.bit(0).sl());
    testdouts_1[252].write(v_.bit(1).sl());
    testdouts_2[252].write(v_.bit(2).sl());
    testdouts_3[252].write(v_.bit(3).sl());
    testdouts_4[252].write(v_.bit(4).sl());
    testdouts_5[252].write(v_.bit(5).sl());
    testdouts_6[252].write(v_.bit(6).sl());
    testdouts_7[252].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_8 - Lane 2 observability
// OFFSET: 0x933
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx2_pm_error_counter__15__8__ - Error counter value
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_254() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x933ULL))) & vl::rd(rx2_pm_error_counter).slice<8>(8);
    testdouts_0[253].write(v_.bit(0).sl());
    testdouts_1[253].write(v_.bit(1).sl());
    testdouts_2[253].write(v_.bit(2).sl());
    testdouts_3[253].write(v_.bit(3).sl());
    testdouts_4[253].write(v_.bit(4).sl());
    testdouts_5[253].write(v_.bit(5).sl());
    testdouts_6[253].write(v_.bit(6).sl());
    testdouts_7[253].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_9 - Lane 2 observability
// OFFSET: 0x934
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:2] rx2_rxhs_ddl_phase_clk_if__4__0__ - Clock DDL setting chosen
// RESET: 0x00
// [1] rx2_rxhs_ddl_phase_change_p_if - DDL setting change latch pulse
// RESET: 0x00
// [0] rx2_pm_start - Pattern matcher start flag observation
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_255() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x934ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx2_rxhs_ddl_phase_clk_if), vl::rd(rx2_rxhs_ddl_phase_change_p_if), vl::rd(rx2_pm_start));
    testdouts_0[254].write(v_.bit(0).sl());
    testdouts_1[254].write(v_.bit(1).sl());
    testdouts_2[254].write(v_.bit(2).sl());
    testdouts_3[254].write(v_.bit(3).sl());
    testdouts_4[254].write(v_.bit(4).sl());
    testdouts_5[254].write(v_.bit(5).sl());
    testdouts_6[254].write(v_.bit(6).sl());
    testdouts_7[254].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_LANE_10 - Lane 2 observability
// OFFSET: 0x935
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx2_rxhs_ddl_phase_data_if__3__0__ - Data DDl setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_256() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x935ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx2_rxhs_ddl_phase_data_if));
    testdouts_0[255].write(v_.bit(0).sl());
    testdouts_1[255].write(v_.bit(1).sl());
    testdouts_2[255].write(v_.bit(2).sl());
    testdouts_3[255].write(v_.bit(3).sl());
    testdouts_4[255].write(v_.bit(4).sl());
    testdouts_5[255].write(v_.bit(5).sl());
    testdouts_6[255].write(v_.bit(6).sl());
    testdouts_7[255].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_OFFSET_CAL_0 - Lane 2 offset compensation control
// OFFSET: 0x97B
// ATTR: RW, CUST
// FIELDS:
// [7] rx2_rxhs_data_offset_cal_en_ovr_en_rw - Data offset calibration override enable control
// RESET: 0x00
// [6] rx2_cal_done_ovr_rw - Calibration done override enable control
// RESET: 0x00
// [5] rx2_cal_done_ovr_en_rw - Calibration done override enable control
// RESET: 0x00
// [4] rx2_start_calibration_ovr_rw - RX start calibration
// RESET: 0x00
// [3] rx2_start_calibration_ovr_en_rw - RX start calibration
// RESET: 0x00
// [2:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_257() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x97bULL))) & vl::cat(vl::rd(rx2_rxhs_data_offset_cal_en_ovr_en_rw), vl::rd(rx2_cal_done_ovr_rw), vl::rd(rx2_cal_done_ovr_en_rw), vl::rd(rx2_start_calibration_ovr_rw), vl::rd(rx2_start_calibration_ovr_en_rw), vl::logic<3>(0x0ULL));
    testdouts_0[256].write(v_.bit(0).sl());
    testdouts_1[256].write(v_.bit(1).sl());
    testdouts_2[256].write(v_.bit(2).sl());
    testdouts_3[256].write(v_.bit(3).sl());
    testdouts_4[256].write(v_.bit(4).sl());
    testdouts_5[256].write(v_.bit(5).sl());
    testdouts_6[256].write(v_.bit(6).sl());
    testdouts_7[256].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_OFFSET_CAL_1 - Lane 2 offset compensation control
// OFFSET: 0x97C
// ATTR: RW, CUST
// FIELDS:
// [7] rx2_rxhs_data_offset_cal_ovr_en_rw - Offset calibration setting override
// RESET: 0x00
// [6:1] rx2_rxhs_data_offset_cal_ovr_rw__5__0__ - Offset calibration setting override enable
// RESET: 0x00
// [0] rx2_rxhs_data_offset_cal_en_ovr_rw - Data offset calibration override enable control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_258() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x97cULL))) & vl::cat(vl::rd(rx2_rxhs_data_offset_cal_ovr_en_rw), vl::rd(rx2_rxhs_data_offset_cal_ovr_rw), vl::rd(rx2_rxhs_data_offset_cal_en_ovr_rw));
    testdouts_0[257].write(v_.bit(0).sl());
    testdouts_1[257].write(v_.bit(1).sl());
    testdouts_2[257].write(v_.bit(2).sl());
    testdouts_3[257].write(v_.bit(3).sl());
    testdouts_4[257].write(v_.bit(4).sl());
    testdouts_5[257].write(v_.bit(5).sl());
    testdouts_6[257].write(v_.bit(6).sl());
    testdouts_7[257].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_OFFSET_CAL_2 - Lane 2 offset compensation control
// OFFSET: 0x97D
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx2_rxhs_data_ovr_rw__7__0__ - RX HS data override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_259() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x97dULL))) & vl::rd(rx2_rxhs_data_ovr_rw);
    testdouts_0[258].write(v_.bit(0).sl());
    testdouts_1[258].write(v_.bit(1).sl());
    testdouts_2[258].write(v_.bit(2).sl());
    testdouts_3[258].write(v_.bit(3).sl());
    testdouts_4[258].write(v_.bit(4).sl());
    testdouts_5[258].write(v_.bit(5).sl());
    testdouts_6[258].write(v_.bit(6).sl());
    testdouts_7[258].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_OFFSET_CAL_3 - Lane 2 offset compensation control
// OFFSET: 0x97E
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] rx2_rxhs_repeatcal_rw - RX HS repeat calibration
// RESET: 0x00
// [0] rx2_rxhs_data_ovr_en_rw - RX HS data override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_260() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x97eULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(rx2_rxhs_repeatcal_rw), vl::rd(rx2_rxhs_data_ovr_en_rw));
    testdouts_0[259].write(v_.bit(0).sl());
    testdouts_1[259].write(v_.bit(1).sl());
    testdouts_2[259].write(v_.bit(2).sl());
    testdouts_3[259].write(v_.bit(3).sl());
    testdouts_4[259].write(v_.bit(4).sl());
    testdouts_5[259].write(v_.bit(5).sl());
    testdouts_6[259].write(v_.bit(6).sl());
    testdouts_7[259].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_OFFSET_CAL_0 - Lane 2 offset compensation observability
// OFFSET: 0x99F
// ATTR: READ, CUST
// FIELDS:
// [7:3] reserved_0 - Reserved field
// RESET: 0x00
// [2] rx2_cal_error - Offset calibration algorithm error
// RESET: 0x00
// [1] rx2_cal_done - Offset calibration algorithm done
// RESET: 0x00
// [0] rx2_start_calibration - Offset calibration algorithm start
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_261() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x99fULL))) & vl::cat(vl::logic<5>(0x0ULL), vl::rd(rx2_cal_error), vl::rd(rx2_cal_done), vl::rd(rx2_start_calibration));
    testdouts_0[260].write(v_.bit(0).sl());
    testdouts_1[260].write(v_.bit(1).sl());
    testdouts_2[260].write(v_.bit(2).sl());
    testdouts_3[260].write(v_.bit(3).sl());
    testdouts_4[260].write(v_.bit(4).sl());
    testdouts_5[260].write(v_.bit(5).sl());
    testdouts_6[260].write(v_.bit(6).sl());
    testdouts_7[260].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_OFFSET_CAL_1 - Lane 2 offset compensation observability
// OFFSET: 0x9A0
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx2_rxhs_data_int__7__0__ - HS-RX data received (AFE output)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_262() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9a0ULL))) & vl::rd(rx2_rxhs_data_int);
    testdouts_0[261].write(v_.bit(0).sl());
    testdouts_1[261].write(v_.bit(1).sl());
    testdouts_2[261].write(v_.bit(2).sl());
    testdouts_3[261].write(v_.bit(3).sl());
    testdouts_4[261].write(v_.bit(4).sl());
    testdouts_5[261].write(v_.bit(5).sl());
    testdouts_6[261].write(v_.bit(6).sl());
    testdouts_7[261].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_OFFSET_CAL_2 - Lane 2 offset compensation observability
// OFFSET: 0x9A1
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] rx2_rxhs_data_offset_cal_en - Offset calibration algorithm enable
// RESET: 0x00
// [5:0] rx2_rxhs_data_offset_cal__5__0__ - Offset calibration algorithm setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_263() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9a1ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx2_rxhs_data_offset_cal_en), vl::rd(rx2_rxhs_data_offset_cal));
    testdouts_0[262].write(v_.bit(0).sl());
    testdouts_1[262].write(v_.bit(1).sl());
    testdouts_2[262].write(v_.bit(2).sl());
    testdouts_3[262].write(v_.bit(3).sl());
    testdouts_4[262].write(v_.bit(4).sl());
    testdouts_5[262].write(v_.bit(5).sl());
    testdouts_6[262].write(v_.bit(6).sl());
    testdouts_7[262].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_OFFSET_CAL_3 - Lane 2 offset compensation observability
// OFFSET: 0x9A2
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx2_total_ones__7__0__ - Offset calibration algorithm counter of 1s
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_264() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9a2ULL))) & vl::rd(rx2_total_ones);
    testdouts_0[263].write(v_.bit(0).sl());
    testdouts_1[263].write(v_.bit(1).sl());
    testdouts_2[263].write(v_.bit(2).sl());
    testdouts_3[263].write(v_.bit(3).sl());
    testdouts_4[263].write(v_.bit(4).sl());
    testdouts_5[263].write(v_.bit(5).sl());
    testdouts_6[263].write(v_.bit(6).sl());
    testdouts_7[263].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_DDL_0 - Lane 2 DDL tune observability
// OFFSET: 0x9E0
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rx2_ddl_start_calibration_if - DDL tune digital algorithm start
// RESET: 0x00
// [2] rx2_ddl_finished_if - DDL tune done indication
// RESET: 0x00
// [1] rx2_ddl_error_flag_if - DDL tune error
// RESET: 0x00
// [0] ddl_tune_en_lane2_if - DDL tune analog circuitry enable indication
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_265() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9e0ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx2_ddl_start_calibration_if), vl::rd(rx2_ddl_finished_if), vl::rd(rx2_ddl_error_flag_if), vl::rd(ddl_tune_en_lane2_if));
    testdouts_0[264].write(v_.bit(0).sl());
    testdouts_1[264].write(v_.bit(1).sl());
    testdouts_2[264].write(v_.bit(2).sl());
    testdouts_3[264].write(v_.bit(3).sl());
    testdouts_4[264].write(v_.bit(4).sl());
    testdouts_5[264].write(v_.bit(5).sl());
    testdouts_6[264].write(v_.bit(6).sl());
    testdouts_7[264].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_DDL_1 - Lane 2 DDL tune observability
// OFFSET: 0x9E1
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx2_freq_meas_out_if__7__0__ - RX DDL tunning oscillation measurement
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_266() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9e1ULL))) & vl::rd(rx2_freq_meas_out_if).slice<8>(0);
    testdouts_0[265].write(v_.bit(0).sl());
    testdouts_1[265].write(v_.bit(1).sl());
    testdouts_2[265].write(v_.bit(2).sl());
    testdouts_3[265].write(v_.bit(3).sl());
    testdouts_4[265].write(v_.bit(4).sl());
    testdouts_5[265].write(v_.bit(5).sl());
    testdouts_6[265].write(v_.bit(6).sl());
    testdouts_7[265].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_DDL_2 - Lane 2 DDL tune observability
// OFFSET: 0x9E2
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx2_freq_meas_out_if__11__8__ - RX DDL tunning oscillation measurement
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_267() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9e2ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx2_freq_meas_out_if).slice<4>(8));
    testdouts_0[266].write(v_.bit(0).sl());
    testdouts_1[266].write(v_.bit(1).sl());
    testdouts_2[266].write(v_.bit(2).sl());
    testdouts_3[266].write(v_.bit(3).sl());
    testdouts_4[266].write(v_.bit(4).sl());
    testdouts_5[266].write(v_.bit(5).sl());
    testdouts_6[266].write(v_.bit(6).sl());
    testdouts_7[266].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_DDL_3 - Lane 2 DDL tune observability
// OFFSET: 0x9E3
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx2_osc_freq_target_if__7__0__ - RX oscillator frequency target obs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_268() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9e3ULL))) & vl::rd(rx2_osc_freq_target_if).slice<8>(0);
    testdouts_0[267].write(v_.bit(0).sl());
    testdouts_1[267].write(v_.bit(1).sl());
    testdouts_2[267].write(v_.bit(2).sl());
    testdouts_3[267].write(v_.bit(3).sl());
    testdouts_4[267].write(v_.bit(4).sl());
    testdouts_5[267].write(v_.bit(5).sl());
    testdouts_6[267].write(v_.bit(6).sl());
    testdouts_7[267].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_DDL_4 - Lane 2 DDL tune observability
// OFFSET: 0x9E4
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx2_osc_freq_target_if__11__8__ - RX oscillator frequency target obs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_269() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9e4ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx2_osc_freq_target_if).slice<4>(8));
    testdouts_0[268].write(v_.bit(0).sl());
    testdouts_1[268].write(v_.bit(1).sl());
    testdouts_2[268].write(v_.bit(2).sl());
    testdouts_3[268].write(v_.bit(3).sl());
    testdouts_4[268].write(v_.bit(4).sl());
    testdouts_5[268].write(v_.bit(5).sl());
    testdouts_6[268].write(v_.bit(6).sl());
    testdouts_7[268].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE2_DDL_5 - Lane 2 DDL tune observability
// OFFSET: 0x9E5
// ATTR: READ, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4:0] rx2_rxhs_ddl_tune_if__4__0__ - RX HS DDL tunning setting
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_270() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0x9e5ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::logic<1>(0x0ULL), vl::rd(rx2_rxhs_ddl_tune_if));
    testdouts_0[269].write(v_.bit(0).sl());
    testdouts_1[269].write(v_.bit(1).sl());
    testdouts_2[269].write(v_.bit(2).sl());
    testdouts_3[269].write(v_.bit(3).sl());
    testdouts_4[269].write(v_.bit(4).sl());
    testdouts_5[269].write(v_.bit(5).sl());
    testdouts_6[269].write(v_.bit(6).sl());
    testdouts_7[269].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_DDL_0 - Lane 2 DDL tune control
// OFFSET: 0xA06
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx2_rxhs_ddl_tune_en_if_rw - RX HS DDL tune enable
// RESET: 0x00
// [4] rx2_rxhs_ddl_tune_en_bypass_rw - RX HS DDL tune enable bypass
// RESET: 0x00
// [3] rx2_ddl_start_calibration_ovr_rw - RX DDL tunning calibration start override
// RESET: 0x00
// [2] rx2_ddl_start_calibration_ovr_en_rw - RX DDL tunning calibration start override enable
// RESET: 0x00
// [1] rx2_ddl_finished_ovr_rw - RX DDL tunning done override
// RESET: 0x00
// [0] rx2_ddl_finished_ovr_en_rw - RX DDL tunning done override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_271() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xa06ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx2_rxhs_ddl_tune_en_if_rw), vl::rd(rx2_rxhs_ddl_tune_en_bypass_rw), vl::rd(rx2_ddl_start_calibration_ovr_rw), vl::rd(rx2_ddl_start_calibration_ovr_en_rw), vl::rd(rx2_ddl_finished_ovr_rw), vl::rd(rx2_ddl_finished_ovr_en_rw));
    testdouts_0[270].write(v_.bit(0).sl());
    testdouts_1[270].write(v_.bit(1).sl());
    testdouts_2[270].write(v_.bit(2).sl());
    testdouts_3[270].write(v_.bit(3).sl());
    testdouts_4[270].write(v_.bit(4).sl());
    testdouts_5[270].write(v_.bit(5).sl());
    testdouts_6[270].write(v_.bit(6).sl());
    testdouts_7[270].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_DDL_1 - Lane 2 DDL tune control
// OFFSET: 0xA07
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx2_rxhs_ddl_tune_ovr_en_rw - RX HS DDL tune override enable control
// RESET: 0x00
// [4:0] rx2_rxhs_ddl_tune_ovr_rw__4__0__ - RX HS DDL tune override for lane 2
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_272() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xa07ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx2_rxhs_ddl_tune_ovr_en_rw), vl::rd(rx2_rxhs_ddl_tune_ovr_rw));
    testdouts_0[271].write(v_.bit(0).sl());
    testdouts_1[271].write(v_.bit(1).sl());
    testdouts_2[271].write(v_.bit(2).sl());
    testdouts_3[271].write(v_.bit(3).sl());
    testdouts_4[271].write(v_.bit(4).sl());
    testdouts_5[271].write(v_.bit(5).sl());
    testdouts_6[271].write(v_.bit(6).sl());
    testdouts_7[271].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_DDL_4 - Lane 2 DDL tune control
// OFFSET: 0xA0A
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx2_osc_freq_target_if_ovr_rw__7__0__ - Oscillating frequency override Target
// RESET: 0x77
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_273() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xa0aULL))) & vl::rd(rx2_osc_freq_target_if_ovr_rw).slice<8>(0);
    testdouts_0[272].write(v_.bit(0).sl());
    testdouts_1[272].write(v_.bit(1).sl());
    testdouts_2[272].write(v_.bit(2).sl());
    testdouts_3[272].write(v_.bit(3).sl());
    testdouts_4[272].write(v_.bit(4).sl());
    testdouts_5[272].write(v_.bit(5).sl());
    testdouts_6[272].write(v_.bit(6).sl());
    testdouts_7[272].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_DDL_5 - Lane 2 DDL tune control
// OFFSET: 0xA0B
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx2_osc_freq_target_if_ovr_rw__11__8__ - Oscillating frequency override Target
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_274() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xa0bULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx2_osc_freq_target_if_ovr_rw).slice<4>(8));
    testdouts_0[273].write(v_.bit(0).sl());
    testdouts_1[273].write(v_.bit(1).sl());
    testdouts_2[273].write(v_.bit(2).sl());
    testdouts_3[273].write(v_.bit(3).sl());
    testdouts_4[273].write(v_.bit(4).sl());
    testdouts_5[273].write(v_.bit(5).sl());
    testdouts_6[273].write(v_.bit(6).sl());
    testdouts_7[273].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE2_DDL_6 - Lane 2 DDL tune control
// OFFSET: 0xA0C
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] rx2_osc_freq_target_if_ovr_en_rw - Override for oscillation frequency target
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_275() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xa0cULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(rx2_osc_freq_target_if_ovr_en_rw));
    testdouts_0[274].write(v_.bit(0).sl());
    testdouts_1[274].write(v_.bit(1).sl());
    testdouts_2[274].write(v_.bit(2).sl());
    testdouts_3[274].write(v_.bit(3).sl());
    testdouts_4[274].write(v_.bit(4).sl());
    testdouts_5[274].write(v_.bit(5).sl());
    testdouts_6[274].write(v_.bit(6).sl());
    testdouts_7[274].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_0 - Lane 3 control
// OFFSET: 0xB01
// ATTR: RW, CUST
// FIELDS:
// [7] hsrx_en_bypass_lane3_rw - HS RX enable bypass
// RESET: 0x00
// [6] bintpon_lane3_rw - Bias block power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [5] bintpon_bypass_lane3_rw - Bias block power-on bypass override
// RESET: 0x00
// [4:1] atb_sel_lane3_rw__3__0__ - Analog Test Bus signals selection:
// RESET: 0x00
// [0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_276() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb01ULL))) & vl::cat(vl::rd(hsrx_en_bypass_lane3_rw), vl::rd(bintpon_lane3_rw), vl::rd(bintpon_bypass_lane3_rw), vl::rd(atb_sel_lane3_rw), vl::logic<1>(0x0ULL));
    testdouts_0[275].write(v_.bit(0).sl());
    testdouts_1[275].write(v_.bit(1).sl());
    testdouts_2[275].write(v_.bit(2).sl());
    testdouts_3[275].write(v_.bit(3).sl());
    testdouts_4[275].write(v_.bit(4).sl());
    testdouts_5[275].write(v_.bit(5).sl());
    testdouts_6[275].write(v_.bit(6).sl());
    testdouts_7[275].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_1 - Lane 3 control
// OFFSET: 0xB02
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] hsrxpon_if_lane3_rw - Power on of the RX HS
// RESET: 0x00
// [3] hsrxpon_bypass_lane3_rw - Power on of the RX HS bypass control
// RESET: 0x00
// [2] hsrxenterm_if_lane3_rw - RX HS enable term
// RESET: 0x00
// [1] hsrxenterm_bypass_lane3_rw - Bypass control of RX high speed enable term
// RESET: 0x00
// [0] hsrx_en_if_lane3_tester_rw - HS RX enable for tester
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_277() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb02ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(hsrxpon_if_lane3_rw), vl::rd(hsrxpon_bypass_lane3_rw), vl::rd(hsrxenterm_if_lane3_rw), vl::rd(hsrxenterm_bypass_lane3_rw), vl::rd(hsrx_en_if_lane3_tester_rw));
    testdouts_0[276].write(v_.bit(0).sl());
    testdouts_1[276].write(v_.bit(1).sl());
    testdouts_2[276].write(v_.bit(2).sl());
    testdouts_3[276].write(v_.bit(3).sl());
    testdouts_4[276].write(v_.bit(4).sl());
    testdouts_5[276].write(v_.bit(5).sl());
    testdouts_6[276].write(v_.bit(6).sl());
    testdouts_7[276].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_3 - Lane 3 control
// OFFSET: 0xB04
// ATTR: RW, CUST
// FIELDS:
// [7] lprxponcd_bypass_lane3_rw - LP RX Contention Detector power-on bypass override
// RESET: 0x00
// [6:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_278() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb04ULL))) & vl::cat(vl::rd(lprxponcd_bypass_lane3_rw), vl::logic<7>(0x0ULL));
    testdouts_0[277].write(v_.bit(0).sl());
    testdouts_1[277].write(v_.bit(1).sl());
    testdouts_2[277].write(v_.bit(2).sl());
    testdouts_3[277].write(v_.bit(3).sl());
    testdouts_4[277].write(v_.bit(4).sl());
    testdouts_5[277].write(v_.bit(5).sl());
    testdouts_6[277].write(v_.bit(6).sl());
    testdouts_7[277].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_4 - Lane 3 control
// OFFSET: 0xB05
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] lprxponulp_lane3_rw - LP RX ULP power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [3] lprxponulp_bypass_lane3_rw - LP RX ULP power-on bypass override
// RESET: 0x00
// [2] lprxponlp_lane3_rw - LP RX LP power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
// [1] lprxponlp_bypass_lane3_rw - LP RX LP power-on bypass override
// RESET: 0x00
// [0] lprxponcd_lane3_rw - LP RX Contention Detector power-on; can be overridden if bypass signal is 1'b1
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_279() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb05ULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(lprxponulp_lane3_rw), vl::rd(lprxponulp_bypass_lane3_rw), vl::rd(lprxponlp_lane3_rw), vl::rd(lprxponlp_bypass_lane3_rw), vl::rd(lprxponcd_lane3_rw));
    testdouts_0[278].write(v_.bit(0).sl());
    testdouts_1[278].write(v_.bit(1).sl());
    testdouts_2[278].write(v_.bit(2).sl());
    testdouts_3[278].write(v_.bit(3).sl());
    testdouts_4[278].write(v_.bit(4).sl());
    testdouts_5[278].write(v_.bit(5).sl());
    testdouts_6[278].write(v_.bit(6).sl());
    testdouts_7[278].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_6 - Lane 3 control
// OFFSET: 0xB07
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx3_rxhs_feed_int_clk_ovr_en_rw - HS-RX source clock override enable control for lane 3. Selects between internal clock source or external (receiver)
// RESET: 0x00
// [4] rx3_rxhs_feed_int_clk_ovr_rw - HS-RX source clock override control for lane 3. Selects between internal clock source or external (receiver)
// RESET: 0x00
// [3:1] reserved_1 - Reserved field
// RESET: 0x00
// [0] polarity_lane3_rw -
// NOTES: Polarity change control
// <br>0 - Two PHYs with P/N ports connected to P/N respectively
// <br>1 - Two PHYs with P/N ports connected to N/P respectively
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_280() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb07ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx3_rxhs_feed_int_clk_ovr_en_rw), vl::rd(rx3_rxhs_feed_int_clk_ovr_rw), vl::logic<3>(0x0ULL), vl::rd(polarity_lane3_rw));
    testdouts_0[279].write(v_.bit(0).sl());
    testdouts_1[279].write(v_.bit(1).sl());
    testdouts_2[279].write(v_.bit(2).sl());
    testdouts_3[279].write(v_.bit(3).sl());
    testdouts_4[279].write(v_.bit(4).sl());
    testdouts_5[279].write(v_.bit(5).sl());
    testdouts_6[279].write(v_.bit(6).sl());
    testdouts_7[279].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_7 - Lane 3 control
// OFFSET: 0xB08
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx3_rxhs_clk_from_long_channel_if_rw - RX HS clock from long channel
// RESET: 0x00
// [4] recal_lane3_ovr_rw - Override Enable voltage control for recal
// RESET: 0x00
// [3] recal_lane3_ovr_en_rw - Override Enable voltage control for recal
// RESET: 0x00
// [2:0] reserved_1 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_281() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb08ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx3_rxhs_clk_from_long_channel_if_rw), vl::rd(recal_lane3_ovr_rw), vl::rd(recal_lane3_ovr_en_rw), vl::logic<3>(0x0ULL));
    testdouts_0[280].write(v_.bit(0).sl());
    testdouts_1[280].write(v_.bit(1).sl());
    testdouts_2[280].write(v_.bit(2).sl());
    testdouts_3[280].write(v_.bit(3).sl());
    testdouts_4[280].write(v_.bit(4).sl());
    testdouts_5[280].write(v_.bit(5).sl());
    testdouts_6[280].write(v_.bit(6).sl());
    testdouts_7[280].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_8 - Lane 3 control
// OFFSET: 0xB09
// ATTR: RW, CUST
// FIELDS:
// [7] rx_des_en_lane3_rw - RX deserializer enable
// RESET: 0x00
// [6] rx_des_en_bypass_lane3_rw - RX deserializer enable bypass control
// RESET: 0x00
// [5:3] reserved_0 - Reserved field
// RESET: 0x00
// [2:0] rx3_rxhs_data_eq_if_rw__2__0__ - RX HS data equalizer
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_282() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb09ULL))) & vl::cat(vl::rd(rx_des_en_lane3_rw), vl::rd(rx_des_en_bypass_lane3_rw), vl::logic<3>(0x0ULL), vl::rd(rx3_rxhs_data_eq_if_rw));
    testdouts_0[281].write(v_.bit(0).sl());
    testdouts_1[281].write(v_.bit(1).sl());
    testdouts_2[281].write(v_.bit(2).sl());
    testdouts_3[281].write(v_.bit(3).sl());
    testdouts_4[281].write(v_.bit(4).sl());
    testdouts_5[281].write(v_.bit(5).sl());
    testdouts_6[281].write(v_.bit(6).sl());
    testdouts_7[281].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_9 - Lane 3 control
// OFFSET: 0xB0A
// ATTR: RW, CUST
// FIELDS:
// [7] rx3_pm_sample_error_counter_rw - strobe to read the error counter
// RESET: 0x00
// [6] rx3_pm_start_control_ovr_en_rw - Pattern matcher start override enable.
// RESET: 0x00
// [5] rx3_pm_start_control_ovr_rw - Pattern matcher start override.
// RESET: 0x00
// [4:1] rx3_pm_mode_r_rw__3__0__ - Configuration mode for the pattern matcher
// RESET: 0x07
// [0] rx3_data_retiming_out_swap_sel_rw - retiming mux control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_283() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0aULL))) & vl::cat(vl::rd(rx3_pm_sample_error_counter_rw), vl::rd(rx3_pm_start_control_ovr_en_rw), vl::rd(rx3_pm_start_control_ovr_rw), vl::rd(rx3_pm_mode_r_rw), vl::rd(rx3_data_retiming_out_swap_sel_rw));
    testdouts_0[282].write(v_.bit(0).sl());
    testdouts_1[282].write(v_.bit(1).sl());
    testdouts_2[282].write(v_.bit(2).sl());
    testdouts_3[282].write(v_.bit(3).sl());
    testdouts_4[282].write(v_.bit(4).sl());
    testdouts_5[282].write(v_.bit(5).sl());
    testdouts_6[282].write(v_.bit(6).sl());
    testdouts_7[282].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_10 - Lane 3 control
// OFFSET: 0xB0B
// ATTR: RW, CUST
// FIELDS:
// [7] rx3_rxhs_ddl_phase_clk_ovr_en_rw - RX HS DDL phase data override control
// RESET: 0x00
// [6:2] rx3_rxhs_ddl_phase_clk_ovr_rw__4__0__ - RX HS DDL phase clock override
// RESET: 0x00
// [1] rx3_rxhs_ddl_phase_change_p_ovr_en_rw - RX HS DDL phase changed override control
// RESET: 0x00
// [0] rx3_rxhs_ddl_phase_change_p_ovr_rw - RX HS DDL phase changed override control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_284() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0bULL))) & vl::cat(vl::rd(rx3_rxhs_ddl_phase_clk_ovr_en_rw), vl::rd(rx3_rxhs_ddl_phase_clk_ovr_rw), vl::rd(rx3_rxhs_ddl_phase_change_p_ovr_en_rw), vl::rd(rx3_rxhs_ddl_phase_change_p_ovr_rw));
    testdouts_0[283].write(v_.bit(0).sl());
    testdouts_1[283].write(v_.bit(1).sl());
    testdouts_2[283].write(v_.bit(2).sl());
    testdouts_3[283].write(v_.bit(3).sl());
    testdouts_4[283].write(v_.bit(4).sl());
    testdouts_5[283].write(v_.bit(5).sl());
    testdouts_6[283].write(v_.bit(6).sl());
    testdouts_7[283].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_11 - Lane 3 control
// OFFSET: 0xB0C
// ATTR: RW, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx3_rxhs_ddl_phase_data_ovr_en_rw - Override enable for ddl phase data
// RESET: 0x00
// [3:0] rx3_rxhs_ddl_phase_data_ovr_rw__3__0__ - RX HS DDL phase clock override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_285() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0cULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx3_rxhs_ddl_phase_data_ovr_en_rw), vl::rd(rx3_rxhs_ddl_phase_data_ovr_rw));
    testdouts_0[284].write(v_.bit(0).sl());
    testdouts_1[284].write(v_.bit(1).sl());
    testdouts_2[284].write(v_.bit(2).sl());
    testdouts_3[284].write(v_.bit(3).sl());
    testdouts_4[284].write(v_.bit(4).sl());
    testdouts_5[284].write(v_.bit(5).sl());
    testdouts_6[284].write(v_.bit(6).sl());
    testdouts_7[284].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_LANE_12 - Lane 3 control
// OFFSET: 0xB0D
// ATTR: RW, CUST
// FIELDS:
// [7] rx3_pm_man_error_latch_rw - Allows manual pattern matcher error counter latch. Latching is controlled by rx3_pm_sample_error_counter
// RESET: 0x00
// [6:3] rx3_lb_prog_if_rw__3__0__ - lane 3 receiver biasing currents
// RESET: 0x0F
// [2] rx3_lb_res_cm_en_if_rw - lane 3 static termination driver middle point pull down enable
// RESET: 0x00
// [1] rx3_lb_res_dn_en_if_rw - lane 3 static termination driver test dn line enable
// RESET: 0x00
// [0] rx3_lb_res_dp_en_if_rw - lane 3 static termination driver test dp line enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_286() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0dULL))) & vl::cat(vl::rd(rx3_pm_man_error_latch_rw), vl::rd(rx3_lb_prog_if_rw), vl::rd(rx3_lb_res_cm_en_if_rw), vl::rd(rx3_lb_res_dn_en_if_rw), vl::rd(rx3_lb_res_dp_en_if_rw));
    testdouts_0[285].write(v_.bit(0).sl());
    testdouts_1[285].write(v_.bit(1).sl());
    testdouts_2[285].write(v_.bit(2).sl());
    testdouts_3[285].write(v_.bit(3).sl());
    testdouts_4[285].write(v_.bit(4).sl());
    testdouts_5[285].write(v_.bit(5).sl());
    testdouts_6[285].write(v_.bit(6).sl());
    testdouts_7[285].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_0 - Lane 3 observability
// OFFSET: 0xB2B
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rx3_rxhs_deserial_data_en_if - HS-RX deserializer reset observation for lane 3
// RESET: 0x00
// [2] hsrx_en_if_lane3 - HS-RX enable of lane 3
// RESET: 0x00
// [1] enable_lane3 - Signal that passes AFE control from soft macro to lane 3
// RESET: 0x00
// [0] bintpon_if_lane3 - Level shifter enable of lane 3
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_287() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb2bULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx3_rxhs_deserial_data_en_if), vl::rd(hsrx_en_if_lane3), vl::rd(enable_lane3), vl::rd(bintpon_if_lane3));
    testdouts_0[286].write(v_.bit(0).sl());
    testdouts_1[286].write(v_.bit(1).sl());
    testdouts_2[286].write(v_.bit(2).sl());
    testdouts_3[286].write(v_.bit(3).sl());
    testdouts_4[286].write(v_.bit(4).sl());
    testdouts_5[286].write(v_.bit(5).sl());
    testdouts_6[286].write(v_.bit(6).sl());
    testdouts_7[286].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_2 - Lane 3 observability
// OFFSET: 0xB2D
// ATTR: READ, CUST
// FIELDS:
// [7] rx3_rxhs_rx_enterm_if - RX HS enable term
// RESET: 0x00
// [6] rx3_rxhs_rx_enlogic_if - RX HS enlogic
// RESET: 0x00
// [5] rx3_rxhs_rx_en_if - RX HS enable
// RESET: 0x00
// [4:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_288() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb2dULL))) & vl::cat(vl::rd(rx3_rxhs_rx_enterm_if), vl::rd(rx3_rxhs_rx_enlogic_if), vl::rd(rx3_rxhs_rx_en_if), vl::logic<5>(0x0ULL));
    testdouts_0[287].write(v_.bit(0).sl());
    testdouts_1[287].write(v_.bit(1).sl());
    testdouts_2[287].write(v_.bit(2).sl());
    testdouts_3[287].write(v_.bit(3).sl());
    testdouts_4[287].write(v_.bit(4).sl());
    testdouts_5[287].write(v_.bit(5).sl());
    testdouts_6[287].write(v_.bit(6).sl());
    testdouts_7[287].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_3 - Lane 3 observability
// OFFSET: 0xB2E
// ATTR: READ, CUST
// FIELDS:
// [7:6] lprxdoutlp_lane3__1__0__ - LP-RX receiver outputs
// RESET: 0x00
// [5:4] lprxdoutcd_lane3__1__0__ - LP-CD receiver outputs
// RESET: 0x00
// [3] rx3_rxlp_ponulp_if - LP-RX ULP receiver power on
// RESET: 0x00
// [2] rx3_rxlp_ponlp_if - LP-RX LP receiver power on
// RESET: 0x00
// [1] rx3_rxlp_poncd_if - LP-RX contention detector power on
// RESET: 0x00
// [0] rx3_rxhs_rx_pon_if - RX HS power on
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_289() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb2eULL))) & vl::cat(vl::rd(lprxdoutlp_lane3), vl::rd(lprxdoutcd_lane3), vl::rd(rx3_rxlp_ponulp_if), vl::rd(rx3_rxlp_ponlp_if), vl::rd(rx3_rxlp_poncd_if), vl::rd(rx3_rxhs_rx_pon_if));
    testdouts_0[288].write(v_.bit(0).sl());
    testdouts_1[288].write(v_.bit(1).sl());
    testdouts_2[288].write(v_.bit(2).sl());
    testdouts_3[288].write(v_.bit(3).sl());
    testdouts_4[288].write(v_.bit(4).sl());
    testdouts_5[288].write(v_.bit(5).sl());
    testdouts_6[288].write(v_.bit(6).sl());
    testdouts_7[288].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_4 - Lane 3 observability
// OFFSET: 0xB2F
// ATTR: READ, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4] rx3_deskewcalfailed_if - Deskew calibration failed signal
// RESET: 0x00
// [3] rx3_deskewcaldone_if - Deskew calibration don
// RESET: 0x00
// [2] lsrxclk_lane3 - Low speed clock (XOR'ed from data lines)
// RESET: 0x00
// [1:0] lprxdoutulp_lane3__1__0__ - LP-RX ULP receiver outputs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_290() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb2fULL))) & vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx3_deskewcalfailed_if), vl::rd(rx3_deskewcaldone_if), vl::rd(lsrxclk_lane3), vl::rd(lprxdoutulp_lane3));
    testdouts_0[289].write(v_.bit(0).sl());
    testdouts_1[289].write(v_.bit(1).sl());
    testdouts_2[289].write(v_.bit(2).sl());
    testdouts_3[289].write(v_.bit(3).sl());
    testdouts_4[289].write(v_.bit(4).sl());
    testdouts_5[289].write(v_.bit(5).sl());
    testdouts_6[289].write(v_.bit(6).sl());
    testdouts_7[289].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_5 - Lane 3 observability
// OFFSET: 0xB30
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx3_rxhs_data_if__7__0__ - RX HS data
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_291() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb30ULL))) & vl::rd(rx3_rxhs_data_if);
    testdouts_0[290].write(v_.bit(0).sl());
    testdouts_1[290].write(v_.bit(1).sl());
    testdouts_2[290].write(v_.bit(2).sl());
    testdouts_3[290].write(v_.bit(3).sl());
    testdouts_4[290].write(v_.bit(4).sl());
    testdouts_5[290].write(v_.bit(5).sl());
    testdouts_6[290].write(v_.bit(6).sl());
    testdouts_7[290].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_6 - Lane 3 observability
// OFFSET: 0xB31
// ATTR: READ, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx3_pm_error_pm - Observation of the error flag generated by the master
// RESET: 0x00
// [4:3] reserved_1 - Reserved field
// RESET: 0x00
// [2] rx3_rxhs_word_clk_if - RX HS word clock
// RESET: 0x00
// [1] rx3_rxhs_feed_int_clk_if - RX HS internal clock feedback
// RESET: 0x00
// [0] rx3_rxhs_des_en_if - RX HS deserializer enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_292() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb31ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx3_pm_error_pm), vl::logic<2>(0x0ULL), vl::rd(rx3_rxhs_word_clk_if), vl::rd(rx3_rxhs_feed_int_clk_if), vl::rd(rx3_rxhs_des_en_if));
    testdouts_0[291].write(v_.bit(0).sl());
    testdouts_1[291].write(v_.bit(1).sl());
    testdouts_2[291].write(v_.bit(2).sl());
    testdouts_3[291].write(v_.bit(3).sl());
    testdouts_4[291].write(v_.bit(4).sl());
    testdouts_5[291].write(v_.bit(5).sl());
    testdouts_6[291].write(v_.bit(6).sl());
    testdouts_7[291].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_7 - Lane 3 observability
// OFFSET: 0xB32
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx3_pm_error_counter__7__0__ - Error counter value
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_293() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb32ULL))) & vl::rd(rx3_pm_error_counter).slice<8>(0);
    testdouts_0[292].write(v_.bit(0).sl());
    testdouts_1[292].write(v_.bit(1).sl());
    testdouts_2[292].write(v_.bit(2).sl());
    testdouts_3[292].write(v_.bit(3).sl());
    testdouts_4[292].write(v_.bit(4).sl());
    testdouts_5[292].write(v_.bit(5).sl());
    testdouts_6[292].write(v_.bit(6).sl());
    testdouts_7[292].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_8 - Lane 3 observability
// OFFSET: 0xB33
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx3_pm_error_counter__15__8__ - Error counter value
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_294() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb33ULL))) & vl::rd(rx3_pm_error_counter).slice<8>(8);
    testdouts_0[293].write(v_.bit(0).sl());
    testdouts_1[293].write(v_.bit(1).sl());
    testdouts_2[293].write(v_.bit(2).sl());
    testdouts_3[293].write(v_.bit(3).sl());
    testdouts_4[293].write(v_.bit(4).sl());
    testdouts_5[293].write(v_.bit(5).sl());
    testdouts_6[293].write(v_.bit(6).sl());
    testdouts_7[293].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_9 - Lane 3 observability
// OFFSET: 0xB34
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6:2] rx3_rxhs_ddl_phase_clk_if__4__0__ - Clock DDL setting chosen
// RESET: 0x00
// [1] rx3_rxhs_ddl_phase_change_p_if - DDL setting change latch pulse
// RESET: 0x00
// [0] rx3_pm_start - Pattern matcher start flag observation
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_295() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb34ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx3_rxhs_ddl_phase_clk_if), vl::rd(rx3_rxhs_ddl_phase_change_p_if), vl::rd(rx3_pm_start));
    testdouts_0[294].write(v_.bit(0).sl());
    testdouts_1[294].write(v_.bit(1).sl());
    testdouts_2[294].write(v_.bit(2).sl());
    testdouts_3[294].write(v_.bit(3).sl());
    testdouts_4[294].write(v_.bit(4).sl());
    testdouts_5[294].write(v_.bit(5).sl());
    testdouts_6[294].write(v_.bit(6).sl());
    testdouts_7[294].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_LANE_10 - Lane 3 observability
// OFFSET: 0xB35
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx3_rxhs_ddl_phase_data_if__3__0__ - Data DDl setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_296() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb35ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx3_rxhs_ddl_phase_data_if));
    testdouts_0[295].write(v_.bit(0).sl());
    testdouts_1[295].write(v_.bit(1).sl());
    testdouts_2[295].write(v_.bit(2).sl());
    testdouts_3[295].write(v_.bit(3).sl());
    testdouts_4[295].write(v_.bit(4).sl());
    testdouts_5[295].write(v_.bit(5).sl());
    testdouts_6[295].write(v_.bit(6).sl());
    testdouts_7[295].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_OFFSET_CAL_0 - Lane 3 offset compensation control
// OFFSET: 0xB7B
// ATTR: RW, CUST
// FIELDS:
// [7] rx3_rxhs_data_offset_cal_en_ovr_en_rw - Data offset calibration override enable control
// RESET: 0x00
// [6] rx3_cal_done_ovr_rw - Calibration done override enable control
// RESET: 0x00
// [5] rx3_cal_done_ovr_en_rw - Calibration done override enable control
// RESET: 0x00
// [4] rx3_start_calibration_ovr_rw - RX start calibration
// RESET: 0x00
// [3] rx3_start_calibration_ovr_en_rw - RX start calibration
// RESET: 0x00
// [2:0] reserved_0 - Reserved field
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_297() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7bULL))) & vl::cat(vl::rd(rx3_rxhs_data_offset_cal_en_ovr_en_rw), vl::rd(rx3_cal_done_ovr_rw), vl::rd(rx3_cal_done_ovr_en_rw), vl::rd(rx3_start_calibration_ovr_rw), vl::rd(rx3_start_calibration_ovr_en_rw), vl::logic<3>(0x0ULL));
    testdouts_0[296].write(v_.bit(0).sl());
    testdouts_1[296].write(v_.bit(1).sl());
    testdouts_2[296].write(v_.bit(2).sl());
    testdouts_3[296].write(v_.bit(3).sl());
    testdouts_4[296].write(v_.bit(4).sl());
    testdouts_5[296].write(v_.bit(5).sl());
    testdouts_6[296].write(v_.bit(6).sl());
    testdouts_7[296].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_OFFSET_CAL_1 - Lane 3 offset compensation control
// OFFSET: 0xB7C
// ATTR: RW, CUST
// FIELDS:
// [7] rx3_rxhs_data_offset_cal_ovr_en_rw - Offset calibration setting override
// RESET: 0x00
// [6:1] rx3_rxhs_data_offset_cal_ovr_rw__5__0__ - Offset calibration setting override enable
// RESET: 0x00
// [0] rx3_rxhs_data_offset_cal_en_ovr_rw - Data offset calibration override enable control
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_298() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7cULL))) & vl::cat(vl::rd(rx3_rxhs_data_offset_cal_ovr_en_rw), vl::rd(rx3_rxhs_data_offset_cal_ovr_rw), vl::rd(rx3_rxhs_data_offset_cal_en_ovr_rw));
    testdouts_0[297].write(v_.bit(0).sl());
    testdouts_1[297].write(v_.bit(1).sl());
    testdouts_2[297].write(v_.bit(2).sl());
    testdouts_3[297].write(v_.bit(3).sl());
    testdouts_4[297].write(v_.bit(4).sl());
    testdouts_5[297].write(v_.bit(5).sl());
    testdouts_6[297].write(v_.bit(6).sl());
    testdouts_7[297].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_OFFSET_CAL_2 - Lane 3 offset compensation control
// OFFSET: 0xB7D
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx3_rxhs_data_ovr_rw__7__0__ - RX HS data override
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_299() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7dULL))) & vl::rd(rx3_rxhs_data_ovr_rw);
    testdouts_0[298].write(v_.bit(0).sl());
    testdouts_1[298].write(v_.bit(1).sl());
    testdouts_2[298].write(v_.bit(2).sl());
    testdouts_3[298].write(v_.bit(3).sl());
    testdouts_4[298].write(v_.bit(4).sl());
    testdouts_5[298].write(v_.bit(5).sl());
    testdouts_6[298].write(v_.bit(6).sl());
    testdouts_7[298].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_OFFSET_CAL_3 - Lane 3 offset compensation control
// OFFSET: 0xB7E
// ATTR: RW, CUST
// FIELDS:
// [7:2] reserved_0 - Reserved field
// RESET: 0x00
// [1] rx3_rxhs_repeatcal_rw - RX HS repeat calibration
// RESET: 0x00
// [0] rx3_rxhs_data_ovr_en_rw - RX HS data override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_300() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7eULL))) & vl::cat(vl::logic<6>(0x0ULL), vl::rd(rx3_rxhs_repeatcal_rw), vl::rd(rx3_rxhs_data_ovr_en_rw));
    testdouts_0[299].write(v_.bit(0).sl());
    testdouts_1[299].write(v_.bit(1).sl());
    testdouts_2[299].write(v_.bit(2).sl());
    testdouts_3[299].write(v_.bit(3).sl());
    testdouts_4[299].write(v_.bit(4).sl());
    testdouts_5[299].write(v_.bit(5).sl());
    testdouts_6[299].write(v_.bit(6).sl());
    testdouts_7[299].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_OFFSET_CAL_0 - Lane 3 offset compensation observability
// OFFSET: 0xB9F
// ATTR: READ, CUST
// FIELDS:
// [7:3] reserved_0 - Reserved field
// RESET: 0x00
// [2] rx3_cal_error - Offset calibration algorithm error
// RESET: 0x00
// [1] rx3_cal_done - Offset calibration algorithm done
// RESET: 0x00
// [0] rx3_start_calibration - Offset calibration algorithm start
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_301() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xb9fULL))) & vl::cat(vl::logic<5>(0x0ULL), vl::rd(rx3_cal_error), vl::rd(rx3_cal_done), vl::rd(rx3_start_calibration));
    testdouts_0[300].write(v_.bit(0).sl());
    testdouts_1[300].write(v_.bit(1).sl());
    testdouts_2[300].write(v_.bit(2).sl());
    testdouts_3[300].write(v_.bit(3).sl());
    testdouts_4[300].write(v_.bit(4).sl());
    testdouts_5[300].write(v_.bit(5).sl());
    testdouts_6[300].write(v_.bit(6).sl());
    testdouts_7[300].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_OFFSET_CAL_1 - Lane 3 offset compensation observability
// OFFSET: 0xBA0
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx3_rxhs_data_int__7__0__ - HS-RX data received (AFE output)
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_302() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xba0ULL))) & vl::rd(rx3_rxhs_data_int);
    testdouts_0[301].write(v_.bit(0).sl());
    testdouts_1[301].write(v_.bit(1).sl());
    testdouts_2[301].write(v_.bit(2).sl());
    testdouts_3[301].write(v_.bit(3).sl());
    testdouts_4[301].write(v_.bit(4).sl());
    testdouts_5[301].write(v_.bit(5).sl());
    testdouts_6[301].write(v_.bit(6).sl());
    testdouts_7[301].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_OFFSET_CAL_2 - Lane 3 offset compensation observability
// OFFSET: 0xBA1
// ATTR: READ, CUST
// FIELDS:
// [7] reserved_0 - Reserved field
// RESET: 0x00
// [6] rx3_rxhs_data_offset_cal_en - Offset calibration algorithm enable
// RESET: 0x00
// [5:0] rx3_rxhs_data_offset_cal__5__0__ - Offset calibration algorithm setting chosen
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_303() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xba1ULL))) & vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx3_rxhs_data_offset_cal_en), vl::rd(rx3_rxhs_data_offset_cal));
    testdouts_0[302].write(v_.bit(0).sl());
    testdouts_1[302].write(v_.bit(1).sl());
    testdouts_2[302].write(v_.bit(2).sl());
    testdouts_3[302].write(v_.bit(3).sl());
    testdouts_4[302].write(v_.bit(4).sl());
    testdouts_5[302].write(v_.bit(5).sl());
    testdouts_6[302].write(v_.bit(6).sl());
    testdouts_7[302].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_OFFSET_CAL_3 - Lane 3 offset compensation observability
// OFFSET: 0xBA2
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx3_total_ones__7__0__ - Offset calibration algorithm counter of 1s
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_304() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xba2ULL))) & vl::rd(rx3_total_ones);
    testdouts_0[303].write(v_.bit(0).sl());
    testdouts_1[303].write(v_.bit(1).sl());
    testdouts_2[303].write(v_.bit(2).sl());
    testdouts_3[303].write(v_.bit(3).sl());
    testdouts_4[303].write(v_.bit(4).sl());
    testdouts_5[303].write(v_.bit(5).sl());
    testdouts_6[303].write(v_.bit(6).sl());
    testdouts_7[303].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_DDL_0 - Lane 3 DDL tune observability
// OFFSET: 0xBE0
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3] rx3_ddl_start_calibration_if - DDL tune digital algorithm start
// RESET: 0x00
// [2] rx3_ddl_finished_if - DDL tune done indication
// RESET: 0x00
// [1] rx3_ddl_error_flag_if - DDL tune error
// RESET: 0x00
// [0] ddl_tune_en_lane3_if - DDL tune analog circuitry enable indication
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_305() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xbe0ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx3_ddl_start_calibration_if), vl::rd(rx3_ddl_finished_if), vl::rd(rx3_ddl_error_flag_if), vl::rd(ddl_tune_en_lane3_if));
    testdouts_0[304].write(v_.bit(0).sl());
    testdouts_1[304].write(v_.bit(1).sl());
    testdouts_2[304].write(v_.bit(2).sl());
    testdouts_3[304].write(v_.bit(3).sl());
    testdouts_4[304].write(v_.bit(4).sl());
    testdouts_5[304].write(v_.bit(5).sl());
    testdouts_6[304].write(v_.bit(6).sl());
    testdouts_7[304].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_DDL_1 - Lane 3 DDL tune observability
// OFFSET: 0xBE1
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx3_freq_meas_out_if__7__0__ - RX DDL tunning oscillation measurement
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_306() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xbe1ULL))) & vl::rd(rx3_freq_meas_out_if).slice<8>(0);
    testdouts_0[305].write(v_.bit(0).sl());
    testdouts_1[305].write(v_.bit(1).sl());
    testdouts_2[305].write(v_.bit(2).sl());
    testdouts_3[305].write(v_.bit(3).sl());
    testdouts_4[305].write(v_.bit(4).sl());
    testdouts_5[305].write(v_.bit(5).sl());
    testdouts_6[305].write(v_.bit(6).sl());
    testdouts_7[305].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_DDL_2 - Lane 3 DDL tune observability
// OFFSET: 0xBE2
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx3_freq_meas_out_if__11__8__ - RX DDL tunning oscillation measurement
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_307() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xbe2ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx3_freq_meas_out_if).slice<4>(8));
    testdouts_0[306].write(v_.bit(0).sl());
    testdouts_1[306].write(v_.bit(1).sl());
    testdouts_2[306].write(v_.bit(2).sl());
    testdouts_3[306].write(v_.bit(3).sl());
    testdouts_4[306].write(v_.bit(4).sl());
    testdouts_5[306].write(v_.bit(5).sl());
    testdouts_6[306].write(v_.bit(6).sl());
    testdouts_7[306].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_DDL_3 - Lane 3 DDL tune observability
// OFFSET: 0xBE3
// ATTR: READ, CUST
// FIELDS:
// [7:0] rx3_osc_freq_target_if__7__0__ - RX oscillator frequency target obs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_308() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xbe3ULL))) & vl::rd(rx3_osc_freq_target_if).slice<8>(0);
    testdouts_0[307].write(v_.bit(0).sl());
    testdouts_1[307].write(v_.bit(1).sl());
    testdouts_2[307].write(v_.bit(2).sl());
    testdouts_3[307].write(v_.bit(3).sl());
    testdouts_4[307].write(v_.bit(4).sl());
    testdouts_5[307].write(v_.bit(5).sl());
    testdouts_6[307].write(v_.bit(6).sl());
    testdouts_7[307].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_DDL_4 - Lane 3 DDL tune observability
// OFFSET: 0xBE4
// ATTR: READ, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx3_osc_freq_target_if__11__8__ - RX oscillator frequency target obs
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_309() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xbe4ULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx3_osc_freq_target_if).slice<4>(8));
    testdouts_0[308].write(v_.bit(0).sl());
    testdouts_1[308].write(v_.bit(1).sl());
    testdouts_2[308].write(v_.bit(2).sl());
    testdouts_3[308].write(v_.bit(3).sl());
    testdouts_4[308].write(v_.bit(4).sl());
    testdouts_5[308].write(v_.bit(5).sl());
    testdouts_6[308].write(v_.bit(6).sl());
    testdouts_7[308].write(v_.bit(7).sl());
}

// %%CREG RD_RX_LANE3_DDL_5 - Lane 3 DDL tune observability
// OFFSET: 0xBE5
// ATTR: READ, CUST
// FIELDS:
// [7:5] reserved_0 - Reserved field
// RESET: 0x00
// [4:0] rx3_rxhs_ddl_tune_if__4__0__ - RX HS DDL tunning setting
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_310() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xbe5ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::logic<1>(0x0ULL), vl::rd(rx3_rxhs_ddl_tune_if));
    testdouts_0[309].write(v_.bit(0).sl());
    testdouts_1[309].write(v_.bit(1).sl());
    testdouts_2[309].write(v_.bit(2).sl());
    testdouts_3[309].write(v_.bit(3).sl());
    testdouts_4[309].write(v_.bit(4).sl());
    testdouts_5[309].write(v_.bit(5).sl());
    testdouts_6[309].write(v_.bit(6).sl());
    testdouts_7[309].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_DDL_0 - Lane 3 DDL tune control
// OFFSET: 0xC06
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx3_rxhs_ddl_tune_en_if_rw - RX HS DDL tune enable
// RESET: 0x00
// [4] rx3_rxhs_ddl_tune_en_bypass_rw - RX HS DDL tune enable bypass
// RESET: 0x00
// [3] rx3_ddl_start_calibration_ovr_rw - RX DDL tunning calibration start override
// RESET: 0x00
// [2] rx3_ddl_start_calibration_ovr_en_rw - RX DDL tunning calibration start override enable
// RESET: 0x00
// [1] rx3_ddl_finished_ovr_rw - RX DDL tunning done override
// RESET: 0x00
// [0] rx3_ddl_finished_ovr_en_rw - RX DDL tunning done override enable
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_311() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xc06ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx3_rxhs_ddl_tune_en_if_rw), vl::rd(rx3_rxhs_ddl_tune_en_bypass_rw), vl::rd(rx3_ddl_start_calibration_ovr_rw), vl::rd(rx3_ddl_start_calibration_ovr_en_rw), vl::rd(rx3_ddl_finished_ovr_rw), vl::rd(rx3_ddl_finished_ovr_en_rw));
    testdouts_0[310].write(v_.bit(0).sl());
    testdouts_1[310].write(v_.bit(1).sl());
    testdouts_2[310].write(v_.bit(2).sl());
    testdouts_3[310].write(v_.bit(3).sl());
    testdouts_4[310].write(v_.bit(4).sl());
    testdouts_5[310].write(v_.bit(5).sl());
    testdouts_6[310].write(v_.bit(6).sl());
    testdouts_7[310].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_DDL_1 - Lane 3 DDL tune control
// OFFSET: 0xC07
// ATTR: RW, CUST
// FIELDS:
// [7:6] reserved_0 - Reserved field
// RESET: 0x00
// [5] rx3_rxhs_ddl_tune_ovr_en_rw - RX HS DDL tune override enable control
// RESET: 0x00
// [4:0] rx3_rxhs_ddl_tune_ovr_rw__4__0__ - RX HS DDL tune override for lane 3
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_312() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xc07ULL))) & vl::cat(vl::logic<2>(0x0ULL), vl::rd(rx3_rxhs_ddl_tune_ovr_en_rw), vl::rd(rx3_rxhs_ddl_tune_ovr_rw));
    testdouts_0[311].write(v_.bit(0).sl());
    testdouts_1[311].write(v_.bit(1).sl());
    testdouts_2[311].write(v_.bit(2).sl());
    testdouts_3[311].write(v_.bit(3).sl());
    testdouts_4[311].write(v_.bit(4).sl());
    testdouts_5[311].write(v_.bit(5).sl());
    testdouts_6[311].write(v_.bit(6).sl());
    testdouts_7[311].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_DDL_4 - Lane 3 DDL tune control
// OFFSET: 0xC0A
// ATTR: RW, CUST
// FIELDS:
// [7:0] rx3_osc_freq_target_if_ovr_rw__7__0__ - Oscillating frequency override Target
// RESET: 0x77
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_313() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xc0aULL))) & vl::rd(rx3_osc_freq_target_if_ovr_rw).slice<8>(0);
    testdouts_0[312].write(v_.bit(0).sl());
    testdouts_1[312].write(v_.bit(1).sl());
    testdouts_2[312].write(v_.bit(2).sl());
    testdouts_3[312].write(v_.bit(3).sl());
    testdouts_4[312].write(v_.bit(4).sl());
    testdouts_5[312].write(v_.bit(5).sl());
    testdouts_6[312].write(v_.bit(6).sl());
    testdouts_7[312].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_DDL_5 - Lane 3 DDL tune control
// OFFSET: 0xC0B
// ATTR: RW, CUST
// FIELDS:
// [7:4] reserved_0 - Reserved field
// RESET: 0x00
// [3:0] rx3_osc_freq_target_if_ovr_rw__11__8__ - Oscillating frequency override Target
// RESET: 0x01
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_314() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xc0bULL))) & vl::cat(vl::logic<4>(0x0ULL), vl::rd(rx3_osc_freq_target_if_ovr_rw).slice<4>(8));
    testdouts_0[313].write(v_.bit(0).sl());
    testdouts_1[313].write(v_.bit(1).sl());
    testdouts_2[313].write(v_.bit(2).sl());
    testdouts_3[313].write(v_.bit(3).sl());
    testdouts_4[313].write(v_.bit(4).sl());
    testdouts_5[313].write(v_.bit(5).sl());
    testdouts_6[313].write(v_.bit(6).sl());
    testdouts_7[313].write(v_.bit(7).sl());
}

// %%CREG RDWR_RX_LANE3_DDL_6 - Lane 3 DDL tune control
// OFFSET: 0xC0C
// ATTR: RW, CUST
// FIELDS:
// [7:1] reserved_0 - Reserved field
// RESET: 0x00
// [0] rx3_osc_freq_target_if_ovr_en_rw - Override for oscillation frequency target
// RESET: 0x00
void mipi_4_rx_dphy_ns_dphy4rxtester::assign_testdouts_0_315() {
    const vl::logic<8> v_ = vl::rep<8>(vl::eq(vl::rd(xaddr), vl::logic<12>(0xc0cULL))) & vl::cat(vl::logic<7>(0x0ULL), vl::rd(rx3_osc_freq_target_if_ovr_en_rw));
    testdouts_0[314].write(v_.bit(0).sl());
    testdouts_1[314].write(v_.bit(1).sl());
    testdouts_2[314].write(v_.bit(2).sl());
    testdouts_3[314].write(v_.bit(3).sl());
    testdouts_4[314].write(v_.bit(4).sl());
    testdouts_5[314].write(v_.bit(5).sl());
    testdouts_6[314].write(v_.bit(6).sl());
    testdouts_7[314].write(v_.bit(7).sl());
}

void mipi_4_rx_dphy_ns_dphy4rxtester::proc_always_823() {
    if (vl::rd(testclr).truth_val() == 0) {
        vl::wr(testcode, vl::logic<8>(0x0ULL));
    } else if (vl::rd(testen).test()) {
        vl::wr(testcode, vl::rd(testdin));
    }
}

void mipi_4_rx_dphy_ns_dphy4rxtester::proc_always_839() {
    if (vl::rd(testclr).truth_val() == 0) {
        vl::wr(testcode_x, vl::logic<4>(0x0ULL));
    } else if (vl::land(vl::lnot(vl::rd(testen)), vl::eq(vl::rd(testcode), vl::logic<8>(0x0ULL))).test()) {
        vl::wr(testcode_x, vl::rd(testdin).slice<4>(0));
    }
}

void mipi_4_rx_dphy_ns_dphy4rxtester::proc_always_1354() {
    if (vl::rd(testclr).truth_val() == 0) {
        vl::wr(adc_done_clear_rw, vl::logic<1>(0x0ULL));
        vl::wr(adc_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(aligner_dk_cnf_rw, vl::logic<2>(0x0ULL));
        vl::wr(atb_lptx1200_on_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(atb_sel_clklane_rw, vl::logic<4>(0x0ULL));
        vl::wr(atb_sel_lane0_rw, vl::logic<4>(0x0ULL));
        vl::wr(atb_sel_lane1_rw, vl::logic<4>(0x0ULL));
        vl::wr(atb_sel_lane2_rw, vl::logic<4>(0x0ULL));
        vl::wr(atb_sel_lane3_rw, vl::logic<4>(0x0ULL));
        vl::wr(bintpon_bypass_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_bypass_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_bypass_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_bypass_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(bintpon_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(bist_manual_rw, vl::logic<1>(0x0ULL));
        vl::wr(bist_run_lane0_rw, vl::logic<1>(0x1ULL));
        vl::wr(bist_run_lane1_rw, vl::logic<1>(0x1ULL));
        vl::wr(bist_run_lane2_rw, vl::logic<1>(0x1ULL));
        vl::wr(bist_run_lane3_rw, vl::logic<1>(0x1ULL));
        vl::wr(burnin_cal_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(burnin_cal_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(bypass_ddltunning_machine_rw, vl::logic<1>(0x0ULL));
        vl::wr(bypass_offset_machine_rw, vl::logic<1>(0x0ULL));
        vl::wr(bypass_skew_machine_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_atb_comp_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_atb_comp_clk_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_atb_ext_con_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_atb_on_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_atb_on_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_atb_sel_rw, vl::logic<2>(0x3ULL));
        vl::wr(cb_atb_vbe_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_bg_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_bg_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_cal_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_cal_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_cal_repl_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_cal_repl_ovr_rw, vl::logic<4>(0x0ULL));
        vl::wr(cb_chop_clk_en_rw, vl::logic<1>(0x1ULL));
        vl::wr(cb_dac_prog_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_dac_prog_ovr_rw, vl::logic<10>(0x0ULL));
        vl::wr(cb_en_45_ohm_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_ical_x4_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_ical_x4_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_lpibias_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_lpibias_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_on_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_on_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(cb_sel_chop_clk_rw, vl::logic<1>(0x1ULL));
        vl::wr(cb_sel_v400_prog_rw, vl::logic<3>(0x4ULL));
        vl::wr(cb_sel_vref_lprx_rw, vl::logic<2>(0x1ULL));
        vl::wr(cb_sel_vrefcd_lprx_rw, vl::logic<2>(0x1ULL));
        vl::wr(cb_sel_vreflptx_prog_rw, vl::logic<1>(0x1ULL));
        vl::wr(cb_vref_mpll_reg_sel_rw, vl::logic<3>(0x0ULL));
        vl::wr(clk_en_lanes_bypass_rw, vl::logic<1>(0x0ULL));
        vl::wr(clk_en_lanes_tester_rw, vl::logic<1>(0x0ULL));
        vl::wr(cmp_polarity_rw, vl::logic<1>(0x0ULL));
        vl::wr(comp_after_strobes_rw, vl::logic<1>(0x1ULL));
        vl::wr(count_ref_rw, vl::logic<10>(0x96ULL));
        vl::wr(counter_for_des_en_bypass_rw, vl::logic<1>(0x0ULL));
        vl::wr(counter_for_des_en_config_if_rw, vl::logic<4>(0x0ULL));
        vl::wr(ddl_osc_freq_target_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(ddl_osc_freq_target_ovr_rw, vl::logic<12>(0x0ULL));
        vl::wr(del_datalane_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(del_datalane_ovr_rw, vl::logic<8>(0x0ULL));
        vl::wr(deskew_jump2steps_rw, vl::logic<2>(0x0ULL));
        vl::wr(deskew_latency_rw, vl::logic<2>(0x3ULL));
        vl::wr(deskew_legacyz_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(deskew_legacyz_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(deskew_numedges_rw, vl::logic<8>(0x40ULL));
        vl::wr(deskew_numedges_update_rw, vl::logic<1>(0x1ULL));
        vl::wr(deskew_overflow_rw, vl::logic<1>(0x0ULL));
        vl::wr(deskew_polarity_rw, vl::logic<1>(0x0ULL));
        vl::wr(enable_lanes_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(enable_lanes_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(force_state_bist_rw, vl::logic<1>(0x0ULL));
        vl::wr(force_state_rw, vl::logic<1>(0x0ULL));
        vl::wr(fsm_trigger_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(fsm_trigger_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsdirect_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsfreqrange_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsfreqrange_ovr_rw, vl::logic<7>(0x0ULL));
        vl::wr(hsrx_en_bypass_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_bypass_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_bypass_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_bypass_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_if_clklane_tester_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_if_lane0_tester_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_if_lane1_tester_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_if_lane2_tester_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrx_en_if_lane3_tester_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_bypass_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_bypass_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_bypass_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_bypass_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_if_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_if_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_if_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_if_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxenterm_if_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_bypass_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_bypass_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_bypass_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_bypass_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_if_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_if_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_if_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_if_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxpon_if_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxtclkmisspd_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxtclkmisspd_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(hsrxthssettle_datalanes_ovr_rw, vl::logic<7>(0x0ULL));
        vl::wr(hsrxthssettle_ovr_en_datalanes_rw, vl::logic<1>(0x0ULL));
        vl::wr(lb_pll_bustie_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_bypass_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_bypass_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_bypass_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_bypass_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponcd_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_bypass_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_bypass_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_bypass_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_bypass_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponlp_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_bypass_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_bypass_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_bypass_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_bypass_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(lprxponulp_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(lptxdin_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lptxdin_lane0_rw, vl::logic<2>(0x0ULL));
        vl::wr(lptxena_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lptxena_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lptxenpulldwn_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lptxenpulldwn_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lptxenulp_bypass_lane0_rw, vl::logic<1>(0x1ULL));
        vl::wr(lptxenulp_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lptxpon_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(lptxpon_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(mastermacro_rw, vl::logic<1>(0x1ULL));
        vl::wr(noalign_error_rw, vl::logic<1>(0x0ULL));
        vl::wr(noext_burnin_res_cal_rw, vl::logic<1>(0x0ULL));
        vl::wr(norexttune_done_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(norexttune_done_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(number_of_ready_count_rw, vl::logic<3>(0x1ULL));
        vl::wr(offset_comp_threshold_rw, vl::logic<8>(0x3fULL));
        vl::wr(onpll_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(onpll_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(pg_bist_error_rw, vl::logic<1>(0x0ULL));
        vl::wr(pg_data_polarity_rw, vl::logic<1>(0x0ULL));
        vl::wr(pg_data_swap_rw, vl::logic<1>(0x0ULL));
        vl::wr(pg_mode_r_rw, vl::logic<4>(0x7ULL));
        vl::wr(pg_reg_err_r_rw, vl::logic<1>(0x0ULL));
        vl::wr(pg_usert_pat_rw, vl::logic<8>(0x0ULL));
        vl::wr(pipe_clean_time_rw, vl::logic<8>(0x80ULL));
        vl::wr(pll_atb_sense_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_boot_from_start_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_clkouten_left_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_clkouten_right_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_clksel_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_clksel_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_cpbias_cntrl_rw, vl::logic<7>(0x0ULL));
        vl::wr(pll_force_fsm_state_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_force_lock_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_gear_shift_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_gear_shift_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_gear_shift_time_rw, vl::logic<9>(0x83ULL));
        vl::wr(pll_gmp_cntrl_rw, vl::logic<2>(0x0ULL));
        vl::wr(pll_int_cntrl_rw, vl::logic<6>(0x0ULL));
        vl::wr(pll_lock_det_on_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_lock_det_on_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_lock_det_time_rw, vl::logic<9>(0x3fULL));
        vl::wr(pll_lock_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_lock_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_lock_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_lockdet_mode_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_m_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_m_ovr_rw, vl::logic<10>(0xf8ULL));
        vl::wr(pll_meas_iv_rw, vl::logic<18>(0x0ULL));
        vl::wr(pll_mpll_prog_rw, vl::logic<17>(0x1ULL));
        vl::wr(pll_n_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_n_ovr_rw, vl::logic<4>(0xbULL));
        vl::wr(pll_opmode_rw, vl::logic<5>(0x0ULL));
        vl::wr(pll_prop_cntrl_rw, vl::logic<6>(0x0ULL));
        vl::wr(pll_pwron_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_pwron_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_reset_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_reset_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_rst_time_rw, vl::logic<9>(0x100ULL));
        vl::wr(pll_startup_fsm_state_rw, vl::logic<4>(0x0ULL));
        vl::wr(pll_th1_rw, vl::logic<10>(0x3ULL));
        vl::wr(pll_th2_rw, vl::logic<8>(0x2dULL));
        vl::wr(pll_th3_rw, vl::logic<8>(0x3ULL));
        vl::wr(pll_th_delay_rw, vl::logic<6>(0x18ULL));
        vl::wr(pll_tstplldig_rw, vl::logic<3>(0x0ULL));
        vl::wr(pll_vco_cntrl_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(pll_vco_cntrl_ovr_rw, vl::logic<6>(0x3fULL));
        vl::wr(polarity_clklane_rw, vl::logic<1>(0x0ULL));
        vl::wr(polarity_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(polarity_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(polarity_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(polarity_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(recal_lane0_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(recal_lane0_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(recal_lane1_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(recal_lane1_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(recal_lane2_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(recal_lane2_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(recal_lane3_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(recal_lane3_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_cal_done_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_cal_done_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_data_retiming_out_swap_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_ddl_finished_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_ddl_finished_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_ddl_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_ddl_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_lb_prog_if_rw, vl::logic<4>(0xfULL));
        vl::wr(rx0_lb_res_cm_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_lb_res_dn_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_lb_res_dp_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_osc_freq_target_if_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_osc_freq_target_if_ovr_rw, vl::logic<12>(0x177ULL));
        vl::wr(rx0_pm_man_error_latch_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_pm_mode_r_rw, vl::logic<4>(0x7ULL));
        vl::wr(rx0_pm_sample_error_counter_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_pm_start_control_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_pm_start_control_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_clk_from_long_channel_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_data_eq_if_rw, vl::logic<3>(0x0ULL));
        vl::wr(rx0_rxhs_data_offset_cal_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_data_offset_cal_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_data_offset_cal_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_data_offset_cal_ovr_rw, vl::logic<6>(0x0ULL));
        vl::wr(rx0_rxhs_data_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_data_ovr_rw, vl::logic<8>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_phase_change_p_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_phase_change_p_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_phase_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_phase_clk_ovr_rw, vl::logic<5>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_phase_data_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_phase_data_ovr_rw, vl::logic<4>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_tune_en_bypass_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_tune_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_tune_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_ddl_tune_ovr_rw, vl::logic<5>(0x0ULL));
        vl::wr(rx0_rxhs_feed_int_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_feed_int_clk_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_rxhs_repeatcal_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx0_txlp_eniocont_if_rw, vl::logic<2>(0x0ULL));
        vl::wr(rx0_txlp_sr_bypass_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_cal_done_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_cal_done_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_data_retiming_out_swap_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_ddl_finished_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_ddl_finished_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_ddl_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_ddl_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_lb_prog_if_rw, vl::logic<4>(0xfULL));
        vl::wr(rx1_lb_res_cm_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_lb_res_dn_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_lb_res_dp_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_osc_freq_target_if_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_osc_freq_target_if_ovr_rw, vl::logic<12>(0x177ULL));
        vl::wr(rx1_pm_man_error_latch_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_pm_mode_r_rw, vl::logic<4>(0x7ULL));
        vl::wr(rx1_pm_sample_error_counter_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_pm_start_control_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_pm_start_control_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_clk_from_long_channel_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_data_eq_if_rw, vl::logic<3>(0x0ULL));
        vl::wr(rx1_rxhs_data_offset_cal_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_data_offset_cal_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_data_offset_cal_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_data_offset_cal_ovr_rw, vl::logic<6>(0x0ULL));
        vl::wr(rx1_rxhs_data_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_data_ovr_rw, vl::logic<8>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_phase_change_p_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_phase_change_p_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_phase_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_phase_clk_ovr_rw, vl::logic<5>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_phase_data_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_phase_data_ovr_rw, vl::logic<4>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_tune_en_bypass_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_tune_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_tune_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_ddl_tune_ovr_rw, vl::logic<5>(0x0ULL));
        vl::wr(rx1_rxhs_feed_int_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_feed_int_clk_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_rxhs_repeatcal_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx1_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_cal_done_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_cal_done_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_data_retiming_out_swap_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_ddl_finished_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_ddl_finished_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_ddl_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_ddl_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_lb_prog_if_rw, vl::logic<4>(0xfULL));
        vl::wr(rx2_lb_res_cm_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_lb_res_dn_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_lb_res_dp_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_osc_freq_target_if_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_osc_freq_target_if_ovr_rw, vl::logic<12>(0x177ULL));
        vl::wr(rx2_pm_man_error_latch_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_pm_mode_r_rw, vl::logic<4>(0x7ULL));
        vl::wr(rx2_pm_sample_error_counter_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_pm_start_control_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_pm_start_control_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_clk_from_long_channel_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_data_eq_if_rw, vl::logic<3>(0x0ULL));
        vl::wr(rx2_rxhs_data_offset_cal_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_data_offset_cal_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_data_offset_cal_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_data_offset_cal_ovr_rw, vl::logic<6>(0x0ULL));
        vl::wr(rx2_rxhs_data_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_data_ovr_rw, vl::logic<8>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_phase_change_p_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_phase_change_p_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_phase_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_phase_clk_ovr_rw, vl::logic<5>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_phase_data_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_phase_data_ovr_rw, vl::logic<4>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_tune_en_bypass_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_tune_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_tune_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_ddl_tune_ovr_rw, vl::logic<5>(0x0ULL));
        vl::wr(rx2_rxhs_feed_int_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_feed_int_clk_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_rxhs_repeatcal_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx2_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_cal_done_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_cal_done_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_data_retiming_out_swap_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_ddl_finished_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_ddl_finished_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_ddl_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_ddl_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_lb_prog_if_rw, vl::logic<4>(0xfULL));
        vl::wr(rx3_lb_res_cm_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_lb_res_dn_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_lb_res_dp_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_osc_freq_target_if_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_osc_freq_target_if_ovr_rw, vl::logic<12>(0x177ULL));
        vl::wr(rx3_pm_man_error_latch_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_pm_mode_r_rw, vl::logic<4>(0x7ULL));
        vl::wr(rx3_pm_sample_error_counter_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_pm_start_control_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_pm_start_control_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_clk_from_long_channel_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_data_eq_if_rw, vl::logic<3>(0x0ULL));
        vl::wr(rx3_rxhs_data_offset_cal_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_data_offset_cal_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_data_offset_cal_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_data_offset_cal_ovr_rw, vl::logic<6>(0x0ULL));
        vl::wr(rx3_rxhs_data_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_data_ovr_rw, vl::logic<8>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_phase_change_p_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_phase_change_p_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_phase_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_phase_clk_ovr_rw, vl::logic<5>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_phase_data_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_phase_data_ovr_rw, vl::logic<4>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_tune_en_bypass_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_tune_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_tune_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_ddl_tune_ovr_rw, vl::logic<5>(0x0ULL));
        vl::wr(rx3_rxhs_feed_int_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_feed_int_clk_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_rxhs_repeatcal_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx3_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_cb_ibias_en_if_rw, vl::logic<1>(0x1ULL));
        vl::wr(rx_cb_lb_en_rext_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_cb_lb_setrx_if_rw, vl::logic<3>(0x0ULL));
        vl::wr(rx_cb_pre_iref_on_rw, vl::logic<1>(0x1ULL));
        vl::wr(rx_ddl_offset_error_rw, vl::logic<12>(0x32ULL));
        vl::wr(rx_des_en_bypass_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_des_en_bypass_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_des_en_bypass_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_des_en_bypass_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_des_en_lane0_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_des_en_lane1_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_des_en_lane2_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_des_en_lane3_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_rxhs_compatibility_mode_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_rxhs_compatibility_mode_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_rxhs_ddl_clk_out_div_rw, vl::logic<2>(0x2ULL));
        vl::wr(rx_rxhs_gmode_if_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_rxhs_gmode_if_ovr_rw, vl::logic<2>(0x0ULL));
        vl::wr(rx_rxhs_prog_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_rxhs_prog_ovr_rw, vl::logic<2>(0x0ULL));
        vl::wr(rx_rxhs_setr_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rx_rxhs_setr_ovr_rw, vl::logic<3>(0x0ULL));
        vl::wr(rx_rxlp_bias_prog_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_caldone_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_caldone_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lb_prog_if_rw, vl::logic<4>(0xfULL));
        vl::wr(rxclk_lb_res_cm_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lb_res_dn_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lb_res_dp_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lb_sel_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lb_sel_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_clk_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_clk_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_clk_kill_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_data_if_rw, vl::logic<8>(0xaaULL));
        vl::wr(rxclk_lbck_div_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_div_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_serial_clk_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_serial_clk_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_serial_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbck_serial_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_clk_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_clk_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_clk_kill_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_div_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_div_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_serial_clk_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_serial_clk_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_serial_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_lbdata_serial_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_clk_offset_cal_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_clk_offset_cal_ovr_rw, vl::logic<6>(0x0ULL));
        vl::wr(rxclk_rxhs_clk_to_long_channel_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_ddr_clk_div_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_ddr_clk_div_ovr_rw, vl::logic<2>(0x0ULL));
        vl::wr(rxclk_rxhs_ddr_clk_en_bypass_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_ddr_clk_en_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_ddr_clk_sel_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_feed_int_clk_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_feed_int_clk_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_int_clk_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_osc_pon_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_osc_pon_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_osc_res_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_osc_res_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_pull_long_channel_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_rx_en200mode_if_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_rxhs_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_start_calibration_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxclk_start_calibration_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(rxpontime_rw, vl::logic<8>(0x5ULL));
        vl::wr(setr_max_rw, vl::logic<4>(0xbULL));
        vl::wr(setr_min_rw, vl::logic<4>(0x4ULL));
        vl::wr(setr_typ_rw, vl::logic<4>(0x7ULL));
        vl::wr(skew_back_comp_en_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(skew_back_comp_en_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(skew_mux_run_rw, vl::logic<1>(0x0ULL));
        vl::wr(skew_mux_sel_rw, vl::logic<1>(0x0ULL));
        vl::wr(stability_time_rw, vl::logic<8>(0x0ULL));
        vl::wr(tafailsafez_rw, vl::logic<1>(0x0ULL));
        vl::wr(target_state_bist_rw, vl::logic<4>(0x0ULL));
        vl::wr(target_state_rw, vl::logic<4>(0x0ULL));
        vl::wr(tclk_miss_div2_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(tclk_miss_div2_ovr_rw, vl::logic<1>(0x0ULL));
        vl::wr(tclk_miss_opt_rw, vl::logic<2>(0x3ULL));
        vl::wr(timebase_ovr_en_rw, vl::logic<1>(0x0ULL));
        vl::wr(timebase_ovr_rw, vl::logic<8>(0x0ULL));
        vl::wr(timeinitbg_rw, vl::logic<10>(0x110ULL));
        vl::wr(tolerance_rw, vl::logic<8>(0x2ULL));
        vl::wr(ttago_rw, vl::logic<6>(0x3ULL));
        vl::wr(ttasure_rw, vl::logic<6>(0x1ULL));
        vl::wr(ttawait_rw, vl::logic<6>(0x32ULL));
        vl::wr(twakeupulpm_rw, vl::logic<10>(0x144ULL));
        vl::wr(txdindlytime_rw, vl::logic<10>(0x1cULL));
        vl::wr(txlp_prog_rw, vl::logic<3>(0x0ULL));
        vl::wr(txstarttime_rw, vl::logic<10>(0x110ULL));
    } else if (vl::rd(testen).truth_val() == 0) {
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1ULL)).test()) {
            vl::wr(force_state_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1ULL)).test()) {
            vl::wr(target_state_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1ULL)).test()) {
            vl::wr(hsfreqrange_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x2ULL)).test()) {
            vl::wr(hsfreqrange_ovr_rw, vl::rd(testdin).slice<7>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x2ULL)).test()) {
            vl::wr(timebase_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x3ULL)).test()) {
            vl::wr(timebase_ovr_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x4ULL)).test()) {
            vl::wr(hsdirect_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x4ULL)).test()) {
            vl::wr(noext_burnin_res_cal_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x4ULL)).test()) {
            vl::wr(cmp_polarity_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x5ULL)).test()) {
            vl::wr(number_of_ready_count_rw, vl::rd(testdin).slice<3>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x5ULL)).test()) {
            vl::wr(tclk_miss_opt_rw, vl::rd(testdin).slice<2>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x5ULL)).test()) {
            vl::wr(hsrxtclkmisspd_ovr_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x5ULL)).test()) {
            vl::wr(hsrxtclkmisspd_ovr_en_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x6ULL)).test()) {
            vl::wr(offset_comp_threshold_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x7ULL)).test()) {
            vl::wr(tolerance_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x8ULL)).test()) {
            vl::wr(skew_mux_run_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x8ULL)).test()) {
            vl::wr(skew_mux_sel_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x8ULL)).test()) {
            vl::wr(deskew_latency_rw, vl::rd(testdin).slice<2>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x8ULL)).test()) {
            vl::wr(deskew_polarity_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x8ULL)).test()) {
            vl::wr(tclk_miss_div2_ovr_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x8ULL)).test()) {
            vl::wr(tclk_miss_div2_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x9ULL)).test()) {
            vl::wr(deskew_numedges_update_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x9ULL)).test()) {
            vl::wr(deskew_overflow_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x9ULL)).test()) {
            vl::wr(deskew_jump2steps_rw, vl::rd(testdin).slice<2>(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x9ULL)).test()) {
            vl::wr(noalign_error_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x9ULL)).test()) {
            vl::wr(aligner_dk_cnf_rw, vl::rd(testdin).slice<2>(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x9ULL)).test()) {
            vl::wr(tafailsafez_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xaULL)).test()) {
            vl::wr(deskew_numedges_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37ULL)).test()) {
            rx_ddl_offset_error_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x38ULL)).test()) {
            rx_ddl_offset_error_rw.write_part(8, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x39ULL)).test()) {
            vl::wr(rx_rxhs_ddl_clk_out_div_rw, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x4eULL)).test()) {
            timeinitbg_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x4fULL)).test()) {
            timeinitbg_rw.write_part(8, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50ULL)).test()) {
            vl::wr(ttago_rw, vl::rd(testdin).slice<6>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x51ULL)).test()) {
            vl::wr(ttasure_rw, vl::rd(testdin).slice<6>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x52ULL)).test()) {
            vl::wr(ttawait_rw, vl::rd(testdin).slice<6>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x53ULL)).test()) {
            twakeupulpm_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x54ULL)).test()) {
            twakeupulpm_rw.write_part(8, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x55ULL)).test()) {
            txdindlytime_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x56ULL)).test()) {
            txdindlytime_rw.write_part(8, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57ULL)).test()) {
            txstarttime_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x58ULL)).test()) {
            txstarttime_rw.write_part(8, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x59ULL)).test()) {
            vl::wr(rxpontime_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL)).test()) {
            vl::wr(clk_en_lanes_bypass_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL)).test()) {
            vl::wr(clk_en_lanes_tester_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL)).test()) {
            vl::wr(bypass_offset_machine_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL)).test()) {
            vl::wr(bypass_ddltunning_machine_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL)).test()) {
            vl::wr(skew_back_comp_en_ovr_en_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL)).test()) {
            vl::wr(skew_back_comp_en_ovr_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL)).test()) {
            vl::wr(bypass_skew_machine_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe0ULL)).test()) {
            vl::wr(rx_rxhs_compatibility_mode_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe1ULL)).test()) {
            vl::wr(rx_rxhs_compatibility_mode_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe1ULL)).test()) {
            vl::wr(rxclk_rxhs_ddr_clk_en_bypass_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe1ULL)).test()) {
            vl::wr(enable_lanes_ovr_en_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe1ULL)).test()) {
            vl::wr(enable_lanes_ovr_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe1ULL)).test()) {
            vl::wr(deskew_legacyz_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe1ULL)).test()) {
            vl::wr(deskew_legacyz_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe2ULL)).test()) {
            ddl_osc_freq_target_ovr_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe3ULL)).test()) {
            ddl_osc_freq_target_ovr_rw.write_part(8, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe4ULL)).test()) {
            vl::wr(ddl_osc_freq_target_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe4ULL)).test()) {
            vl::wr(rx_rxhs_gmode_if_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe4ULL)).test()) {
            vl::wr(rx_rxhs_gmode_if_ovr_rw, vl::rd(testdin).slice<2>(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe4ULL)).test()) {
            vl::wr(counter_for_des_en_config_if_rw, vl::rd(testdin).slice<4>(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xe5ULL)).test()) {
            vl::wr(counter_for_des_en_bypass_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xf1ULL)).test()) {
            vl::wr(hsrxthssettle_datalanes_ovr_rw, vl::rd(testdin).slice<7>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xf1ULL)).test()) {
            vl::wr(hsrxthssettle_ovr_en_datalanes_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x107ULL)).test()) {
            vl::wr(pg_reg_err_r_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x107ULL)).test()) {
            vl::wr(pg_data_swap_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x107ULL)).test()) {
            vl::wr(pg_data_polarity_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x107ULL)).test()) {
            vl::wr(pg_bist_error_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x107ULL)).test()) {
            vl::wr(pg_mode_r_rw, vl::rd(testdin).slice<4>(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x108ULL)).test()) {
            vl::wr(pg_usert_pat_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x109ULL)).test()) {
            vl::wr(rxclk_lbck_data_if_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10aULL)).test()) {
            vl::wr(comp_after_strobes_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10aULL)).test()) {
            vl::wr(bist_manual_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10aULL)).test()) {
            vl::wr(bist_run_lane0_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10aULL)).test()) {
            vl::wr(bist_run_lane1_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10aULL)).test()) {
            vl::wr(bist_run_lane2_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10aULL)).test()) {
            vl::wr(bist_run_lane3_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10bULL)).test()) {
            vl::wr(target_state_bist_rw, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10bULL)).test()) {
            vl::wr(force_state_bist_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10bULL)).test()) {
            vl::wr(rxclk_lbdata_clk_kill_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x10bULL)).test()) {
            vl::wr(rxclk_lbck_clk_kill_if_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL)).test()) {
            vl::wr(rxclk_lbck_clk_en_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL)).test()) {
            vl::wr(rxclk_lbck_clk_en_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL)).test()) {
            vl::wr(rxclk_lbck_div_en_ovr_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL)).test()) {
            vl::wr(rxclk_lbck_div_en_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL)).test()) {
            vl::wr(rxclk_lbck_serial_clk_en_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL)).test()) {
            vl::wr(rxclk_lbck_serial_clk_en_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL)).test()) {
            vl::wr(rxclk_lbck_serial_en_ovr_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x110ULL)).test()) {
            vl::wr(rxclk_lbck_serial_en_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL)).test()) {
            vl::wr(rxclk_lbdata_clk_en_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL)).test()) {
            vl::wr(rxclk_lbdata_clk_en_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL)).test()) {
            vl::wr(rxclk_lbdata_div_en_ovr_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL)).test()) {
            vl::wr(rxclk_lbdata_div_en_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL)).test()) {
            vl::wr(rxclk_lbdata_serial_clk_en_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL)).test()) {
            vl::wr(rxclk_lbdata_serial_clk_en_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL)).test()) {
            vl::wr(rxclk_lbdata_serial_en_ovr_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x111ULL)).test()) {
            vl::wr(rxclk_lbdata_serial_en_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x112ULL)).test()) {
            vl::wr(del_datalane_ovr_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x113ULL)).test()) {
            vl::wr(del_datalane_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x113ULL)).test()) {
            vl::wr(fsm_trigger_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x113ULL)).test()) {
            vl::wr(fsm_trigger_ovr_en_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x113ULL)).test()) {
            vl::wr(rxclk_lb_sel_ovr_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x113ULL)).test()) {
            vl::wr(rxclk_lb_sel_ovr_en_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x113ULL)).test()) {
            vl::wr(rx_rxhs_prog_ovr_rw, vl::rd(testdin).slice<2>(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x113ULL)).test()) {
            vl::wr(rx_rxhs_prog_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x114ULL)).test()) {
            vl::wr(pipe_clean_time_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x133ULL)).test()) {
            vl::wr(mastermacro_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15dULL)).test()) {
            vl::wr(onpll_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15dULL)).test()) {
            vl::wr(onpll_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15dULL)).test()) {
            vl::wr(pll_atb_sense_sel_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15dULL)).test()) {
            vl::wr(pll_clksel_en_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15dULL)).test()) {
            vl::wr(pll_clksel_en_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15dULL)).test()) {
            vl::wr(pll_boot_from_start_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15dULL)).test()) {
            vl::wr(lb_pll_bustie_if_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15eULL)).test()) {
            vl::wr(pll_cpbias_cntrl_rw, vl::rd(testdin).slice<7>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15eULL)).test()) {
            vl::wr(pll_force_fsm_state_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15fULL)).test()) {
            vl::wr(pll_force_lock_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15fULL)).test()) {
            vl::wr(pll_gear_shift_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x15fULL)).test()) {
            vl::wr(pll_gear_shift_ovr_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x160ULL)).test()) {
            pll_gear_shift_time_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x161ULL)).test()) {
            pll_gear_shift_time_rw.write_bit(8, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x162ULL)).test()) {
            vl::wr(pll_gmp_cntrl_rw, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x162ULL)).test()) {
            vl::wr(pll_int_cntrl_rw, vl::rd(testdin).slice<6>(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x163ULL)).test()) {
            vl::wr(pll_lockdet_mode_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x163ULL)).test()) {
            vl::wr(pll_lock_det_on_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x163ULL)).test()) {
            vl::wr(pll_lock_det_on_ovr_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x164ULL)).test()) {
            pll_lock_det_time_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x165ULL)).test()) {
            pll_lock_det_time_rw.write_bit(8, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x166ULL)).test()) {
            vl::wr(pll_lock_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x166ULL)).test()) {
            vl::wr(pll_lock_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x166ULL)).test()) {
            vl::wr(pll_lock_sel_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x167ULL)).test()) {
            pll_meas_iv_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x168ULL)).test()) {
            pll_meas_iv_rw.write_part(8, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x169ULL)).test()) {
            pll_meas_iv_rw.write_part(16, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16aULL)).test()) {
            pll_mpll_prog_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16bULL)).test()) {
            pll_mpll_prog_rw.write_part(8, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16cULL)).test()) {
            pll_mpll_prog_rw.write_bit(16, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16dULL)).test()) {
            vl::wr(pll_opmode_rw, vl::rd(testdin).slice<5>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16eULL)).test()) {
            vl::wr(pll_prop_cntrl_rw, vl::rd(testdin).slice<6>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16eULL)).test()) {
            vl::wr(pll_pwron_ovr_en_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16eULL)).test()) {
            vl::wr(pll_pwron_ovr_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16fULL)).test()) {
            vl::wr(pll_reset_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x16fULL)).test()) {
            vl::wr(pll_reset_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x170ULL)).test()) {
            pll_rst_time_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x171ULL)).test()) {
            pll_rst_time_rw.write_bit(8, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x172ULL)).test()) {
            vl::wr(pll_startup_fsm_state_rw, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x173ULL)).test()) {
            pll_th1_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x174ULL)).test()) {
            pll_th1_rw.write_part(8, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x175ULL)).test()) {
            vl::wr(pll_th2_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x176ULL)).test()) {
            vl::wr(pll_th3_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x177ULL)).test()) {
            vl::wr(pll_th_delay_rw, vl::rd(testdin).slice<6>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x178ULL)).test()) {
            vl::wr(pll_tstplldig_rw, vl::rd(testdin).slice<3>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x178ULL)).test()) {
            vl::wr(pll_n_ovr_rw, vl::rd(testdin).slice<4>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x178ULL)).test()) {
            vl::wr(pll_n_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x179ULL)).test()) {
            pll_m_ovr_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x17aULL)).test()) {
            pll_m_ovr_rw.write_part(8, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x17bULL)).test()) {
            vl::wr(pll_m_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x17bULL)).test()) {
            vl::wr(pll_vco_cntrl_ovr_rw, vl::rd(testdin).slice<6>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x17bULL)).test()) {
            vl::wr(pll_vco_cntrl_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x17cULL)).test()) {
            vl::wr(pll_clkouten_left_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x17cULL)).test()) {
            vl::wr(pll_clkouten_right_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aaULL)).test()) {
            vl::wr(cb_chop_clk_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aaULL)).test()) {
            vl::wr(cb_sel_chop_clk_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aaULL)).test()) {
            vl::wr(cb_sel_v400_prog_rw, vl::rd(testdin).slice<3>(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aaULL)).test()) {
            vl::wr(cb_sel_vrefcd_lprx_rw, vl::rd(testdin).slice<2>(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1abULL)).test()) {
            vl::wr(cb_sel_vref_lprx_rw, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1abULL)).test()) {
            vl::wr(cb_sel_vreflptx_prog_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1abULL)).test()) {
            vl::wr(txlp_prog_rw, vl::rd(testdin).slice<3>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1abULL)).test()) {
            vl::wr(cb_atb_vbe_sel_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1acULL)).test()) {
            vl::wr(cb_atb_sel_rw, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1acULL)).test()) {
            vl::wr(cb_atb_ext_con_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1acULL)).test()) {
            vl::wr(rx_cb_pre_iref_on_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1acULL)).test()) {
            vl::wr(rx_rxlp_bias_prog_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1adULL)).test()) {
            vl::wr(cb_vref_mpll_reg_sel_rw, vl::rd(testdin).slice<3>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1adULL)).test()) {
            vl::wr(cb_atb_comp_clk_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1adULL)).test()) {
            vl::wr(cb_atb_comp_clk_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1adULL)).test()) {
            vl::wr(cb_bg_en_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1adULL)).test()) {
            vl::wr(cb_bg_en_ovr_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1adULL)).test()) {
            vl::wr(cb_lpibias_en_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aeULL)).test()) {
            vl::wr(cb_lpibias_en_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aeULL)).test()) {
            vl::wr(cb_on_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aeULL)).test()) {
            vl::wr(cb_on_ovr_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1aeULL)).test()) {
            vl::wr(cb_atb_on_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1afULL)).test()) {
            vl::wr(cb_atb_on_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1b0ULL)).test()) {
            vl::wr(rx_cb_ibias_en_if_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1b0ULL)).test()) {
            vl::wr(rx_cb_lb_setrx_if_rw, vl::rd(testdin).slice<3>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1b0ULL)).test()) {
            vl::wr(rx_cb_lb_en_rext_if_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1daULL)).test()) {
            vl::wr(adc_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1daULL)).test()) {
            vl::wr(adc_done_clear_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1dbULL)).test()) {
            cb_dac_prog_ovr_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1dcULL)).test()) {
            cb_dac_prog_ovr_rw.write_part(8, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x1ddULL)).test()) {
            vl::wr(cb_dac_prog_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x209ULL)).test()) {
            vl::wr(cb_cal_en_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x209ULL)).test()) {
            vl::wr(cb_cal_en_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x209ULL)).test()) {
            vl::wr(cb_cal_repl_ovr_rw, vl::rd(testdin).slice<4>(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x20aULL)).test()) {
            vl::wr(cb_cal_repl_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x20aULL)).test()) {
            vl::wr(cb_en_45_ohm_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x20bULL)).test()) {
            vl::wr(cb_ical_x4_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x20bULL)).test()) {
            vl::wr(cb_ical_x4_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x237ULL)).test()) {
            count_ref_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x238ULL)).test()) {
            count_ref_rw.write_part(8, vl::rd(testdin).slice<2>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x239ULL)).test()) {
            vl::wr(setr_max_rw, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x239ULL)).test()) {
            vl::wr(setr_min_rw, vl::rd(testdin).slice<4>(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23aULL)).test()) {
            vl::wr(setr_typ_rw, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23bULL)).test()) {
            vl::wr(stability_time_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23cULL)).test()) {
            vl::wr(rxclk_rxhs_osc_pon_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23cULL)).test()) {
            vl::wr(rxclk_rxhs_osc_pon_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23cULL)).test()) {
            vl::wr(rxclk_rxhs_osc_res_en_ovr_en_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23cULL)).test()) {
            vl::wr(rxclk_rxhs_osc_res_en_ovr_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23cULL)).test()) {
            vl::wr(rx_rxhs_setr_ovr_en_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23cULL)).test()) {
            vl::wr(rx_rxhs_setr_ovr_rw, vl::rd(testdin).slice<3>(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23dULL)).test()) {
            vl::wr(norexttune_done_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23dULL)).test()) {
            vl::wr(norexttune_done_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23dULL)).test()) {
            vl::wr(burnin_cal_en_ovr_en_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x23dULL)).test()) {
            vl::wr(burnin_cal_en_ovr_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x301ULL)).test()) {
            vl::wr(atb_sel_clklane_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x301ULL)).test()) {
            vl::wr(bintpon_bypass_clklane_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x301ULL)).test()) {
            vl::wr(bintpon_clklane_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x301ULL)).test()) {
            vl::wr(hsrx_en_bypass_clklane_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x302ULL)).test()) {
            vl::wr(hsrx_en_if_clklane_tester_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x302ULL)).test()) {
            vl::wr(hsrxenterm_bypass_clklane_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x302ULL)).test()) {
            vl::wr(hsrxenterm_if_clklane_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x302ULL)).test()) {
            vl::wr(hsrxpon_bypass_clklane_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x302ULL)).test()) {
            vl::wr(hsrxpon_if_clklane_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x304ULL)).test()) {
            vl::wr(lprxponcd_bypass_clklane_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x305ULL)).test()) {
            vl::wr(lprxponcd_clklane_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x305ULL)).test()) {
            vl::wr(lprxponlp_bypass_clklane_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x305ULL)).test()) {
            vl::wr(lprxponlp_clklane_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x305ULL)).test()) {
            vl::wr(lprxponulp_bypass_clklane_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x305ULL)).test()) {
            vl::wr(lprxponulp_clklane_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x307ULL)).test()) {
            vl::wr(polarity_clklane_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x307ULL)).test()) {
            vl::wr(rxclk_rxhs_clk_to_long_channel_if_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x307ULL)).test()) {
            vl::wr(rxclk_rxhs_ddr_clk_en_if_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x307ULL)).test()) {
            vl::wr(rxclk_rxhs_feed_int_clk_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x307ULL)).test()) {
            vl::wr(rxclk_rxhs_feed_int_clk_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x307ULL)).test()) {
            vl::wr(rxclk_rxhs_int_clk_sel_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x307ULL)).test()) {
            vl::wr(rxclk_rxhs_pull_long_channel_if_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x308ULL)).test()) {
            vl::wr(rxclk_rxhs_ddr_clk_div_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x308ULL)).test()) {
            vl::wr(rxclk_rxhs_ddr_clk_div_ovr_rw, vl::rd(testdin).slice<2>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x308ULL)).test()) {
            vl::wr(rxclk_rxhs_ddr_clk_sel_if_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x308ULL)).test()) {
            vl::wr(rxclk_rxhs_rx_en200mode_if_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x309ULL)).test()) {
            vl::wr(rxclk_lb_res_dp_en_if_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x309ULL)).test()) {
            vl::wr(rxclk_lb_res_dn_en_if_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x309ULL)).test()) {
            vl::wr(rxclk_lb_res_cm_en_if_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x309ULL)).test()) {
            vl::wr(rxclk_lb_prog_if_rw, vl::rd(testdin).slice<4>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37aULL)).test()) {
            vl::wr(rxclk_caldone_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37aULL)).test()) {
            vl::wr(rxclk_caldone_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37aULL)).test()) {
            vl::wr(rxclk_rxhs_clk_offset_cal_ovr_rw, vl::rd(testdin).slice<6>(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37bULL)).test()) {
            vl::wr(rxclk_rxhs_clk_offset_cal_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37bULL)).test()) {
            vl::wr(rxclk_rxhs_start_calibration_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37bULL)).test()) {
            vl::wr(rxclk_rxhs_start_calibration_ovr_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37bULL)).test()) {
            vl::wr(rxclk_start_calibration_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x37bULL)).test()) {
            vl::wr(rxclk_start_calibration_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x501ULL)).test()) {
            vl::wr(atb_lptx1200_on_lane0_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x501ULL)).test()) {
            vl::wr(atb_sel_lane0_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x501ULL)).test()) {
            vl::wr(bintpon_bypass_lane0_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x501ULL)).test()) {
            vl::wr(bintpon_lane0_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x501ULL)).test()) {
            vl::wr(hsrx_en_bypass_lane0_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x502ULL)).test()) {
            vl::wr(hsrx_en_if_lane0_tester_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x502ULL)).test()) {
            vl::wr(hsrxenterm_bypass_lane0_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x502ULL)).test()) {
            vl::wr(hsrxenterm_if_lane0_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x502ULL)).test()) {
            vl::wr(hsrxpon_bypass_lane0_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x502ULL)).test()) {
            vl::wr(hsrxpon_if_lane0_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x504ULL)).test()) {
            vl::wr(lprxponcd_bypass_lane0_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x505ULL)).test()) {
            vl::wr(lprxponcd_lane0_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x505ULL)).test()) {
            vl::wr(lprxponlp_bypass_lane0_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x505ULL)).test()) {
            vl::wr(lprxponlp_lane0_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x505ULL)).test()) {
            vl::wr(lprxponulp_bypass_lane0_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x505ULL)).test()) {
            vl::wr(lprxponulp_lane0_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x505ULL)).test()) {
            vl::wr(lptxdin_bypass_lane0_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x505ULL)).test()) {
            vl::wr(lptxdin_lane0_rw, vl::rd(testdin).slice<2>(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL)).test()) {
            vl::wr(lptxena_bypass_lane0_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL)).test()) {
            vl::wr(lptxena_lane0_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL)).test()) {
            vl::wr(lptxenpulldwn_bypass_lane0_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL)).test()) {
            vl::wr(lptxenpulldwn_lane0_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL)).test()) {
            vl::wr(lptxenulp_bypass_lane0_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL)).test()) {
            vl::wr(lptxenulp_lane0_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL)).test()) {
            vl::wr(lptxpon_bypass_lane0_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x506ULL)).test()) {
            vl::wr(lptxpon_lane0_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x507ULL)).test()) {
            vl::wr(polarity_lane0_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x507ULL)).test()) {
            vl::wr(rx0_rxhs_feed_int_clk_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x507ULL)).test()) {
            vl::wr(rx0_rxhs_feed_int_clk_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x508ULL)).test()) {
            vl::wr(recal_lane0_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x508ULL)).test()) {
            vl::wr(recal_lane0_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x508ULL)).test()) {
            vl::wr(rx0_rxhs_clk_from_long_channel_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x509ULL)).test()) {
            vl::wr(rx0_rxhs_data_eq_if_rw, vl::rd(testdin).slice<3>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x509ULL)).test()) {
            vl::wr(rx0_txlp_eniocont_if_rw, vl::rd(testdin).slice<2>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x509ULL)).test()) {
            vl::wr(rx0_txlp_sr_bypass_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x509ULL)).test()) {
            vl::wr(rx_des_en_bypass_lane0_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x509ULL)).test()) {
            vl::wr(rx_des_en_lane0_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50aULL)).test()) {
            vl::wr(rx0_data_retiming_out_swap_sel_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50aULL)).test()) {
            vl::wr(rx0_pm_mode_r_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50aULL)).test()) {
            vl::wr(rx0_pm_start_control_ovr_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50aULL)).test()) {
            vl::wr(rx0_pm_start_control_ovr_en_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50aULL)).test()) {
            vl::wr(rx0_pm_sample_error_counter_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50dULL)).test()) {
            vl::wr(rx0_lb_res_dp_en_if_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50dULL)).test()) {
            vl::wr(rx0_lb_res_dn_en_if_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50dULL)).test()) {
            vl::wr(rx0_lb_res_cm_en_if_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50dULL)).test()) {
            vl::wr(rx0_lb_prog_if_rw, vl::rd(testdin).slice<4>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x50dULL)).test()) {
            vl::wr(rx0_pm_man_error_latch_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57bULL)).test()) {
            vl::wr(rx0_start_calibration_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57bULL)).test()) {
            vl::wr(rx0_start_calibration_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57bULL)).test()) {
            vl::wr(rx0_cal_done_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57bULL)).test()) {
            vl::wr(rx0_cal_done_ovr_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57bULL)).test()) {
            vl::wr(rx0_rxhs_data_offset_cal_en_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57cULL)).test()) {
            vl::wr(rx0_rxhs_data_offset_cal_en_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57cULL)).test()) {
            vl::wr(rx0_rxhs_data_offset_cal_ovr_rw, vl::rd(testdin).slice<6>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57cULL)).test()) {
            vl::wr(rx0_rxhs_data_offset_cal_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57dULL)).test()) {
            vl::wr(rx0_rxhs_data_ovr_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57eULL)).test()) {
            vl::wr(rx0_rxhs_data_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x57eULL)).test()) {
            vl::wr(rx0_rxhs_repeatcal_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x606ULL)).test()) {
            vl::wr(rx0_ddl_finished_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x606ULL)).test()) {
            vl::wr(rx0_ddl_finished_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x606ULL)).test()) {
            vl::wr(rx0_ddl_start_calibration_ovr_en_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x606ULL)).test()) {
            vl::wr(rx0_ddl_start_calibration_ovr_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x606ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_tune_en_bypass_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x606ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_tune_en_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x607ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_tune_ovr_rw, vl::rd(testdin).slice<5>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x607ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_tune_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x607ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_phase_change_p_ovr_en_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x607ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_phase_change_p_ovr_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x608ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_phase_clk_ovr_rw, vl::rd(testdin).slice<5>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x608ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_phase_clk_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x609ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_phase_data_ovr_rw, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x609ULL)).test()) {
            vl::wr(rx0_rxhs_ddl_phase_data_ovr_en_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x60aULL)).test()) {
            rx0_osc_freq_target_if_ovr_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x60bULL)).test()) {
            rx0_osc_freq_target_if_ovr_rw.write_part(8, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x60cULL)).test()) {
            vl::wr(rx0_osc_freq_target_if_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x701ULL)).test()) {
            vl::wr(atb_sel_lane1_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x701ULL)).test()) {
            vl::wr(bintpon_bypass_lane1_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x701ULL)).test()) {
            vl::wr(bintpon_lane1_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x701ULL)).test()) {
            vl::wr(hsrx_en_bypass_lane1_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x702ULL)).test()) {
            vl::wr(hsrx_en_if_lane1_tester_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x702ULL)).test()) {
            vl::wr(hsrxenterm_bypass_lane1_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x702ULL)).test()) {
            vl::wr(hsrxenterm_if_lane1_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x702ULL)).test()) {
            vl::wr(hsrxpon_bypass_lane1_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x702ULL)).test()) {
            vl::wr(hsrxpon_if_lane1_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x704ULL)).test()) {
            vl::wr(lprxponcd_bypass_lane1_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x705ULL)).test()) {
            vl::wr(lprxponcd_lane1_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x705ULL)).test()) {
            vl::wr(lprxponlp_bypass_lane1_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x705ULL)).test()) {
            vl::wr(lprxponlp_lane1_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x705ULL)).test()) {
            vl::wr(lprxponulp_bypass_lane1_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x705ULL)).test()) {
            vl::wr(lprxponulp_lane1_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x707ULL)).test()) {
            vl::wr(polarity_lane1_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x707ULL)).test()) {
            vl::wr(rx1_rxhs_feed_int_clk_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x707ULL)).test()) {
            vl::wr(rx1_rxhs_feed_int_clk_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x708ULL)).test()) {
            vl::wr(recal_lane1_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x708ULL)).test()) {
            vl::wr(recal_lane1_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x708ULL)).test()) {
            vl::wr(rx1_rxhs_clk_from_long_channel_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x709ULL)).test()) {
            vl::wr(rx1_rxhs_data_eq_if_rw, vl::rd(testdin).slice<3>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x709ULL)).test()) {
            vl::wr(rx_des_en_bypass_lane1_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x709ULL)).test()) {
            vl::wr(rx_des_en_lane1_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70aULL)).test()) {
            vl::wr(rx1_data_retiming_out_swap_sel_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70aULL)).test()) {
            vl::wr(rx1_pm_mode_r_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70aULL)).test()) {
            vl::wr(rx1_pm_start_control_ovr_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70aULL)).test()) {
            vl::wr(rx1_pm_start_control_ovr_en_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70aULL)).test()) {
            vl::wr(rx1_pm_sample_error_counter_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70bULL)).test()) {
            vl::wr(rx1_rxhs_ddl_phase_change_p_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70bULL)).test()) {
            vl::wr(rx1_rxhs_ddl_phase_change_p_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70bULL)).test()) {
            vl::wr(rx1_rxhs_ddl_phase_clk_ovr_rw, vl::rd(testdin).slice<5>(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70bULL)).test()) {
            vl::wr(rx1_rxhs_ddl_phase_clk_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70cULL)).test()) {
            vl::wr(rx1_rxhs_ddl_phase_data_ovr_rw, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70cULL)).test()) {
            vl::wr(rx1_rxhs_ddl_phase_data_ovr_en_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70dULL)).test()) {
            vl::wr(rx1_lb_res_dp_en_if_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70dULL)).test()) {
            vl::wr(rx1_lb_res_dn_en_if_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70dULL)).test()) {
            vl::wr(rx1_lb_res_cm_en_if_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70dULL)).test()) {
            vl::wr(rx1_lb_prog_if_rw, vl::rd(testdin).slice<4>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x70dULL)).test()) {
            vl::wr(rx1_pm_man_error_latch_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77bULL)).test()) {
            vl::wr(rx1_start_calibration_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77bULL)).test()) {
            vl::wr(rx1_start_calibration_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77bULL)).test()) {
            vl::wr(rx1_cal_done_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77bULL)).test()) {
            vl::wr(rx1_cal_done_ovr_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77bULL)).test()) {
            vl::wr(rx1_rxhs_data_offset_cal_en_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77cULL)).test()) {
            vl::wr(rx1_rxhs_data_offset_cal_en_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77cULL)).test()) {
            vl::wr(rx1_rxhs_data_offset_cal_ovr_rw, vl::rd(testdin).slice<6>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77cULL)).test()) {
            vl::wr(rx1_rxhs_data_offset_cal_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77dULL)).test()) {
            vl::wr(rx1_rxhs_data_ovr_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77eULL)).test()) {
            vl::wr(rx1_rxhs_data_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x77eULL)).test()) {
            vl::wr(rx1_rxhs_repeatcal_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x806ULL)).test()) {
            vl::wr(rx1_ddl_finished_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x806ULL)).test()) {
            vl::wr(rx1_ddl_finished_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x806ULL)).test()) {
            vl::wr(rx1_ddl_start_calibration_ovr_en_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x806ULL)).test()) {
            vl::wr(rx1_ddl_start_calibration_ovr_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x806ULL)).test()) {
            vl::wr(rx1_rxhs_ddl_tune_en_bypass_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x806ULL)).test()) {
            vl::wr(rx1_rxhs_ddl_tune_en_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x807ULL)).test()) {
            vl::wr(rx1_rxhs_ddl_tune_ovr_rw, vl::rd(testdin).slice<5>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x807ULL)).test()) {
            vl::wr(rx1_rxhs_ddl_tune_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x80aULL)).test()) {
            rx1_osc_freq_target_if_ovr_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x80bULL)).test()) {
            rx1_osc_freq_target_if_ovr_rw.write_part(8, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x80cULL)).test()) {
            vl::wr(rx1_osc_freq_target_if_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x901ULL)).test()) {
            vl::wr(atb_sel_lane2_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x901ULL)).test()) {
            vl::wr(bintpon_bypass_lane2_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x901ULL)).test()) {
            vl::wr(bintpon_lane2_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x901ULL)).test()) {
            vl::wr(hsrx_en_bypass_lane2_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x902ULL)).test()) {
            vl::wr(hsrx_en_if_lane2_tester_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x902ULL)).test()) {
            vl::wr(hsrxenterm_bypass_lane2_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x902ULL)).test()) {
            vl::wr(hsrxenterm_if_lane2_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x902ULL)).test()) {
            vl::wr(hsrxpon_bypass_lane2_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x902ULL)).test()) {
            vl::wr(hsrxpon_if_lane2_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x904ULL)).test()) {
            vl::wr(lprxponcd_bypass_lane2_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x905ULL)).test()) {
            vl::wr(lprxponcd_lane2_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x905ULL)).test()) {
            vl::wr(lprxponlp_bypass_lane2_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x905ULL)).test()) {
            vl::wr(lprxponlp_lane2_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x905ULL)).test()) {
            vl::wr(lprxponulp_bypass_lane2_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x905ULL)).test()) {
            vl::wr(lprxponulp_lane2_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x907ULL)).test()) {
            vl::wr(polarity_lane2_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x907ULL)).test()) {
            vl::wr(rx2_rxhs_feed_int_clk_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x907ULL)).test()) {
            vl::wr(rx2_rxhs_feed_int_clk_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x908ULL)).test()) {
            vl::wr(recal_lane2_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x908ULL)).test()) {
            vl::wr(recal_lane2_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x908ULL)).test()) {
            vl::wr(rx2_rxhs_clk_from_long_channel_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x909ULL)).test()) {
            vl::wr(rx2_rxhs_data_eq_if_rw, vl::rd(testdin).slice<3>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x909ULL)).test()) {
            vl::wr(rx_des_en_bypass_lane2_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x909ULL)).test()) {
            vl::wr(rx_des_en_lane2_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90aULL)).test()) {
            vl::wr(rx2_data_retiming_out_swap_sel_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90aULL)).test()) {
            vl::wr(rx2_pm_mode_r_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90aULL)).test()) {
            vl::wr(rx2_pm_start_control_ovr_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90aULL)).test()) {
            vl::wr(rx2_pm_start_control_ovr_en_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90aULL)).test()) {
            vl::wr(rx2_pm_sample_error_counter_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90bULL)).test()) {
            vl::wr(rx2_rxhs_ddl_phase_change_p_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90bULL)).test()) {
            vl::wr(rx2_rxhs_ddl_phase_change_p_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90bULL)).test()) {
            vl::wr(rx2_rxhs_ddl_phase_clk_ovr_rw, vl::rd(testdin).slice<5>(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90bULL)).test()) {
            vl::wr(rx2_rxhs_ddl_phase_clk_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90cULL)).test()) {
            vl::wr(rx2_rxhs_ddl_phase_data_ovr_rw, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90cULL)).test()) {
            vl::wr(rx2_rxhs_ddl_phase_data_ovr_en_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90dULL)).test()) {
            vl::wr(rx2_lb_res_dp_en_if_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90dULL)).test()) {
            vl::wr(rx2_lb_res_dn_en_if_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90dULL)).test()) {
            vl::wr(rx2_lb_res_cm_en_if_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90dULL)).test()) {
            vl::wr(rx2_lb_prog_if_rw, vl::rd(testdin).slice<4>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x90dULL)).test()) {
            vl::wr(rx2_pm_man_error_latch_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97bULL)).test()) {
            vl::wr(rx2_start_calibration_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97bULL)).test()) {
            vl::wr(rx2_start_calibration_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97bULL)).test()) {
            vl::wr(rx2_cal_done_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97bULL)).test()) {
            vl::wr(rx2_cal_done_ovr_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97bULL)).test()) {
            vl::wr(rx2_rxhs_data_offset_cal_en_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97cULL)).test()) {
            vl::wr(rx2_rxhs_data_offset_cal_en_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97cULL)).test()) {
            vl::wr(rx2_rxhs_data_offset_cal_ovr_rw, vl::rd(testdin).slice<6>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97cULL)).test()) {
            vl::wr(rx2_rxhs_data_offset_cal_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97dULL)).test()) {
            vl::wr(rx2_rxhs_data_ovr_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97eULL)).test()) {
            vl::wr(rx2_rxhs_data_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0x97eULL)).test()) {
            vl::wr(rx2_rxhs_repeatcal_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa06ULL)).test()) {
            vl::wr(rx2_ddl_finished_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa06ULL)).test()) {
            vl::wr(rx2_ddl_finished_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa06ULL)).test()) {
            vl::wr(rx2_ddl_start_calibration_ovr_en_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa06ULL)).test()) {
            vl::wr(rx2_ddl_start_calibration_ovr_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa06ULL)).test()) {
            vl::wr(rx2_rxhs_ddl_tune_en_bypass_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa06ULL)).test()) {
            vl::wr(rx2_rxhs_ddl_tune_en_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa07ULL)).test()) {
            vl::wr(rx2_rxhs_ddl_tune_ovr_rw, vl::rd(testdin).slice<5>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa07ULL)).test()) {
            vl::wr(rx2_rxhs_ddl_tune_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa0aULL)).test()) {
            rx2_osc_freq_target_if_ovr_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa0bULL)).test()) {
            rx2_osc_freq_target_if_ovr_rw.write_part(8, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xa0cULL)).test()) {
            vl::wr(rx2_osc_freq_target_if_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb01ULL)).test()) {
            vl::wr(atb_sel_lane3_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb01ULL)).test()) {
            vl::wr(bintpon_bypass_lane3_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb01ULL)).test()) {
            vl::wr(bintpon_lane3_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb01ULL)).test()) {
            vl::wr(hsrx_en_bypass_lane3_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb02ULL)).test()) {
            vl::wr(hsrx_en_if_lane3_tester_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb02ULL)).test()) {
            vl::wr(hsrxenterm_bypass_lane3_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb02ULL)).test()) {
            vl::wr(hsrxenterm_if_lane3_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb02ULL)).test()) {
            vl::wr(hsrxpon_bypass_lane3_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb02ULL)).test()) {
            vl::wr(hsrxpon_if_lane3_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb04ULL)).test()) {
            vl::wr(lprxponcd_bypass_lane3_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb05ULL)).test()) {
            vl::wr(lprxponcd_lane3_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb05ULL)).test()) {
            vl::wr(lprxponlp_bypass_lane3_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb05ULL)).test()) {
            vl::wr(lprxponlp_lane3_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb05ULL)).test()) {
            vl::wr(lprxponulp_bypass_lane3_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb05ULL)).test()) {
            vl::wr(lprxponulp_lane3_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb07ULL)).test()) {
            vl::wr(polarity_lane3_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb07ULL)).test()) {
            vl::wr(rx3_rxhs_feed_int_clk_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb07ULL)).test()) {
            vl::wr(rx3_rxhs_feed_int_clk_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb08ULL)).test()) {
            vl::wr(recal_lane3_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb08ULL)).test()) {
            vl::wr(recal_lane3_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb08ULL)).test()) {
            vl::wr(rx3_rxhs_clk_from_long_channel_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb09ULL)).test()) {
            vl::wr(rx3_rxhs_data_eq_if_rw, vl::rd(testdin).slice<3>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb09ULL)).test()) {
            vl::wr(rx_des_en_bypass_lane3_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb09ULL)).test()) {
            vl::wr(rx_des_en_lane3_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0aULL)).test()) {
            vl::wr(rx3_data_retiming_out_swap_sel_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0aULL)).test()) {
            vl::wr(rx3_pm_mode_r_rw, vl::rd(testdin).slice<4>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0aULL)).test()) {
            vl::wr(rx3_pm_start_control_ovr_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0aULL)).test()) {
            vl::wr(rx3_pm_start_control_ovr_en_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0aULL)).test()) {
            vl::wr(rx3_pm_sample_error_counter_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0bULL)).test()) {
            vl::wr(rx3_rxhs_ddl_phase_change_p_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0bULL)).test()) {
            vl::wr(rx3_rxhs_ddl_phase_change_p_ovr_en_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0bULL)).test()) {
            vl::wr(rx3_rxhs_ddl_phase_clk_ovr_rw, vl::rd(testdin).slice<5>(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0bULL)).test()) {
            vl::wr(rx3_rxhs_ddl_phase_clk_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0cULL)).test()) {
            vl::wr(rx3_rxhs_ddl_phase_data_ovr_rw, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0cULL)).test()) {
            vl::wr(rx3_rxhs_ddl_phase_data_ovr_en_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0dULL)).test()) {
            vl::wr(rx3_lb_res_dp_en_if_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0dULL)).test()) {
            vl::wr(rx3_lb_res_dn_en_if_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0dULL)).test()) {
            vl::wr(rx3_lb_res_cm_en_if_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0dULL)).test()) {
            vl::wr(rx3_lb_prog_if_rw, vl::rd(testdin).slice<4>(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb0dULL)).test()) {
            vl::wr(rx3_pm_man_error_latch_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7bULL)).test()) {
            vl::wr(rx3_start_calibration_ovr_en_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7bULL)).test()) {
            vl::wr(rx3_start_calibration_ovr_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7bULL)).test()) {
            vl::wr(rx3_cal_done_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7bULL)).test()) {
            vl::wr(rx3_cal_done_ovr_rw, vl::rd(testdin).bit(6));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7bULL)).test()) {
            vl::wr(rx3_rxhs_data_offset_cal_en_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7cULL)).test()) {
            vl::wr(rx3_rxhs_data_offset_cal_en_ovr_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7cULL)).test()) {
            vl::wr(rx3_rxhs_data_offset_cal_ovr_rw, vl::rd(testdin).slice<6>(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7cULL)).test()) {
            vl::wr(rx3_rxhs_data_offset_cal_ovr_en_rw, vl::rd(testdin).bit(7));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7dULL)).test()) {
            vl::wr(rx3_rxhs_data_ovr_rw, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7eULL)).test()) {
            vl::wr(rx3_rxhs_data_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xb7eULL)).test()) {
            vl::wr(rx3_rxhs_repeatcal_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc06ULL)).test()) {
            vl::wr(rx3_ddl_finished_ovr_en_rw, vl::rd(testdin).bit(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc06ULL)).test()) {
            vl::wr(rx3_ddl_finished_ovr_rw, vl::rd(testdin).bit(1));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc06ULL)).test()) {
            vl::wr(rx3_ddl_start_calibration_ovr_en_rw, vl::rd(testdin).bit(2));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc06ULL)).test()) {
            vl::wr(rx3_ddl_start_calibration_ovr_rw, vl::rd(testdin).bit(3));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc06ULL)).test()) {
            vl::wr(rx3_rxhs_ddl_tune_en_bypass_rw, vl::rd(testdin).bit(4));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc06ULL)).test()) {
            vl::wr(rx3_rxhs_ddl_tune_en_if_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc07ULL)).test()) {
            vl::wr(rx3_rxhs_ddl_tune_ovr_rw, vl::rd(testdin).slice<5>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc07ULL)).test()) {
            vl::wr(rx3_rxhs_ddl_tune_ovr_en_rw, vl::rd(testdin).bit(5));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc0aULL)).test()) {
            rx3_osc_freq_target_if_ovr_rw.write_part(0, vl::rd(testdin));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc0bULL)).test()) {
            rx3_osc_freq_target_if_ovr_rw.write_part(8, vl::rd(testdin).slice<4>(0));
        }
        if (vl::eq(vl::rd(xaddr), vl::logic<12>(0xc0cULL)).test()) {
            vl::wr(rx3_osc_freq_target_if_ovr_en_rw, vl::rd(testdin).bit(0));
        }
    }
}
