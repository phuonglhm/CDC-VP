# FX1 ISP real-RAW regression baseline

FX1 ISP real-RAW regression baseline: SHA-256 of every frame's NV12 (active bytes) and of the statistics readout (after every frame unless --stats-at-end), model and Python reference bit-exact (DEC-05: project reference, not an owner golden).

Generated 2026-11-09T17:09:24 from git 75675cb3c2d1 (dirty tree) by `tools/run_regression.py --write-baseline`. 116 runs, 308 frames; all bit-exact with the reference.

| kind | RAW | preset | output | frame | NV12 SHA-256 |
|---|---|---|---|---|---|
| image | khoid_ground_1_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `05ab4ffdfe7f57f4bb9d6f152aa9cb580c474324eece09b134f57686f949f465` |
| image | khoid_ground_1_raw_2688x1520_5376 | full | 1920x1080 | 0 | `4f4a787bb9ef4732b1e6d32aab20e1c3dcedfe55eeea2a7cd3bfa7cae29df583` |
| image | khoid_ground_1_raw_2688x1520_5376 | full | 1920x1080 | 1 | `bc71c547856ace133a62b6b90e83cc5cd0cb616dc8433ba1b32f7911214a4ec0` |
| image | khoid_ground_1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `8c15462ff816c46d1c4d8bad0451f9f2b434b6b2f8d6e0650d6d3bc19775a627` |
| image | khoid_ground_1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `7118b9c741de042cb564451922a810b14b772902db6fed5d0f36fcd9729f1380` |
| image | khoid_ground_1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `f8870581809293aec3c02c27a7ca7247d495dbf8f209962e8e5c614735f1d231` |
| image | khoid_ground_1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `f8870581809293aec3c02c27a7ca7247d495dbf8f209962e8e5c614735f1d231` |
| image | khoid_ground_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `8d7922731245c83d99656470651976d22bbd1c22b997830aa8bc05ef1de096d5` |
| image | khoid_ground_raw_2688x1520_5376 | full | 1920x1080 | 0 | `500529ce6e20925465b65a265ee62af9ed18c44f4e9166c5224eac0d00803117` |
| image | khoid_ground_raw_2688x1520_5376 | full | 1920x1080 | 1 | `5deced51a40fed1d79808fb4a44d72865f2856b570f026c52635da29c0dc796a` |
| image | khoid_ground_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `44df579ea003085935d75bbac3d6715a09f0887dde54d966e9fc97ea76d37759` |
| image | khoid_ground_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `546e770a9007341922b0bbef1db1d5d60ab5bed725dd420f7dbb59d9e6654440` |
| image | khoid_ground_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `01cc0f4eb1a5779535741ce0e910c521edba409509a38c580900f300de1e11ee` |
| image | khoid_ground_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `dbb676c0aa7b13c31532cb315275382ac39052bef5d8137c577c40ad6f6a65c1` |
| image | khoid_indoor_led_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `f16ab29df8e929c278ad0f7fda4dbacb16bf6508cccd09cf6b9f8e6af2eb6a7d` |
| image | khoid_indoor_led_raw_2688x1520_5376 | full | 1920x1080 | 0 | `cdaf3cfd76c56507bf5104c09e3330047120aecbcdf22832ec53c343bd1a118a` |
| image | khoid_indoor_led_raw_2688x1520_5376 | full | 1920x1080 | 1 | `6f67995539998885769feb85284134a1a70b2018073b6f7a9a47511f3392fe76` |
| image | khoid_indoor_led_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `9dec74c7dbfd0f9cba15f5e77aa570b77202ff0146a570fc2a204673975980cc` |
| image | khoid_indoor_led_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `1c7b8f7ebc7c9eebf368e18a39e94aa4cd3f47eb2d0673b99a8e528cf7a3196e` |
| image | khoid_indoor_led_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `f51b3f9e6ff2ac03f890d1ab44f96b69109f6c76a7fa716a051d0ff1e3b0b491` |
| image | khoid_indoor_led_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `f51b3f9e6ff2ac03f890d1ab44f96b69109f6c76a7fa716a051d0ff1e3b0b491` |
| image | khoid_outdoor1_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `417d0b9bf2c50f46957bf40788a2f9e5e5a8f34ca774888f68f03bce85a753cb` |
| image | khoid_outdoor1_raw_2688x1520_5376 | full | 1920x1080 | 0 | `854b3a3d1ef93add530515531d0da7cfd0fb072916e4798167ec8ac66471f8b9` |
| image | khoid_outdoor1_raw_2688x1520_5376 | full | 1920x1080 | 1 | `9ef055775e1a8191f994282352542aa9212c302f08cf7e04f05619f281ccfe80` |
| image | khoid_outdoor1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `04a1bb05c91262f89fb6fb300698b3220a9abd218a3f34f75e872c268b4dc3b6` |
| image | khoid_outdoor1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `58e972564d7da391aafb509c660f30b80b0def34c894adaeec3bfa3e3a942a63` |
| image | khoid_outdoor1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `7cdf082c22506f95c853746d2bd79ff9b53a85d85f5d843804611deee596778b` |
| image | khoid_outdoor1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `d48c266b7ae91b5bfdb1ec08e86076c757db98363b9df5182e5d21918cf2251d` |
| image | khoid_outdoor2_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `59040c733c7982356a901cd7a15e8b6b96ddbdae948a227c0fe7599225bb491e` |
| image | khoid_outdoor2_raw_2688x1520_5376 | full | 1920x1080 | 0 | `e05921cadb5917008c38ba843e4d6cdd51e695401ce3cf3842096fc579246e36` |
| image | khoid_outdoor2_raw_2688x1520_5376 | full | 1920x1080 | 1 | `9f743bf460afd8c52abb035c735abd8ac1911dadc589b0115c89a8de3a32a868` |
| image | khoid_outdoor2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `6a6ec83956982dcceb3a9f763421516eaa777c11ae12d4d8a275efaa468f48d1` |
| image | khoid_outdoor2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `b83762313773fe18662a75cc7af0f06167f158ca04a6d213aa6f974a413daf37` |
| image | khoid_outdoor2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `a00a1fc81b49c7eab209a5e74e2002618f67bfa31023b93be6d55269bc999d8c` |
| image | khoid_outdoor2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `85034ea756191b7586298d5f576c5bab8c5bf7a1fe2148ed581f408989c2f18a` |
| image | khoid_outdoor3_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `20c8cee8c6edd41d602d7892d40e624b4486965bb1fd56c74b093b2a888beae0` |
| image | khoid_outdoor3_raw_2688x1520_5376 | full | 1920x1080 | 0 | `70e50b03b11ff9b70e90fcbcf95a01c885613036b9d54f021403cf52f677943c` |
| image | khoid_outdoor3_raw_2688x1520_5376 | full | 1920x1080 | 1 | `8984c65dc5549bccd4dbf75d2f22b69c248b2f30d90ee7fd5ff688b8f7718ce0` |
| image | khoid_outdoor3_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `aa5eb63a5ba32b9b741c56985f5f10b6888d66c1250b07bc8e5ce462f1ac069c` |
| image | khoid_outdoor3_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `1e4ac8a0ea11bde63d525fe03861d17e09a2e48f1a66579641ea2be92cf94146` |
| image | khoid_outdoor3_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `3df3e2a68813a0fc55eedec560a209251a4ae9de561c4605e705753a3232a31c` |
| image | khoid_outdoor3_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `3df3e2a68813a0fc55eedec560a209251a4ae9de561c4605e705753a3232a31c` |
| image | khoid_outdoor4_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `a170f25fe9bf20cba429885ddcd15980dff69fe87b8e92275e54aff9ee581758` |
| image | khoid_outdoor4_raw_2688x1520_5376 | full | 1920x1080 | 0 | `09691f8bfb471412a34a59bd2d3f8a0fad9ca0f2e668d1c84dfb14ab8ffabe77` |
| image | khoid_outdoor4_raw_2688x1520_5376 | full | 1920x1080 | 1 | `6b1fcdd34388a19a255b7c1d6a6f935cf6a35161a1235c104eaf2c722a278fb3` |
| image | khoid_outdoor4_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `ff90eae2e44e8853c1deab45ae6b678bc84ba05f0c1fa74432c52a027434bf36` |
| image | khoid_outdoor4_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `803c1e19d2c8a28f2375dd5310c71dddedfde8431266537a11d6a08cf1311988` |
| image | khoid_outdoor4_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `80cd8c7bf2ed169396c3db60b30c7fc7d0899a3741248d6675248de4ad519736` |
| image | khoid_outdoor4_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `b3a8b7dff9ba25678f7f2c6a77eb3293d368cba99971e1e38f7e3d3b9faf7601` |
| image | khoid_outdoor5_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `f50c5ffdcb47ab8107f9ab9bf3f138235a4bbc29bf6ee5bec90f46b65347507a` |
| image | khoid_outdoor5_raw_2688x1520_5376 | full | 1920x1080 | 0 | `daeedf010350d67454124e3889a5e6ce6dc189835b6819a1919774bfb66a06ea` |
| image | khoid_outdoor5_raw_2688x1520_5376 | full | 1920x1080 | 1 | `4d40c0d6e24f30936105622097e585f759e05c1144c8929738abb305f4e4b418` |
| image | khoid_outdoor5_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `3eeb7fb52719e7fce54a31969d6fc5a243373ff2030fdd172bf34957a96f2370` |
| image | khoid_outdoor5_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `73ad6a56de87b551e62bec58b26a49ead45c608cb11f18bcc8db6b046537a20f` |
| image | khoid_outdoor5_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `71580c52b6b9a9bd3d6b16e84dc6f8a59dc2dd91c2e9d90d9984a4fdb0050d80` |
| image | khoid_outdoor5_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `2071ddc9bd75fbfdb2c1f0299fd4f053cf6bf3669b4622c526a5bd18f5420e9d` |
| image | khoid_outdoor6_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `8bb381135590160f458542537073debf06f407d31d159b3d0b8d440429052838` |
| image | khoid_outdoor6_raw_2688x1520_5376 | full | 1920x1080 | 0 | `180253290db25cb6df313c6be87a382950df89a86fa84c204b5ab2d65e8210ea` |
| image | khoid_outdoor6_raw_2688x1520_5376 | full | 1920x1080 | 1 | `a49e77e28f4df32c5a160d9b880181d125ee6677de06ce3ac92d52eed7bda46e` |
| image | khoid_outdoor6_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `0035910b8a65afd9648e687652f85e02fe7a5c27e1c4129bf04186958026993f` |
| image | khoid_outdoor6_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `967cf724e63731d3044ce171fbc0bb88e690ea05b5ae0d1e2ecb742e7e4a4e0a` |
| image | khoid_outdoor6_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `5ad251c1a70246cd3a201d454acb04c6d75f6bcfe5ec1f5ecfc2a9a776a64016` |
| image | khoid_outdoor6_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `eb45bf24de11a0fe9d6e90765c20da313ff95c3391e16d8b55e8b832edf0c45e` |
| image | khoid_outdoor7_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `d91132c0de92e8ffe2aae2a3e12e029c1b7e2db0d4efbaacf72f152cfbaaaefa` |
| image | khoid_outdoor7_raw_2688x1520_5376 | full | 1920x1080 | 0 | `8c8a433f29b2a93715140bd58983994b09b8f61d645df38ac89ae74ba9a5e407` |
| image | khoid_outdoor7_raw_2688x1520_5376 | full | 1920x1080 | 1 | `8de1c69e5755f043054969871b6dcba9d66060463a9a0798e02d27464cc0883d` |
| image | khoid_outdoor7_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `e9b4a95a13cd0281e66b4ef67621d6c73ee1a5da6910d358b9c6f6a8d028cba7` |
| image | khoid_outdoor7_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `8bc57918d5ad53409e22c9b0b1b998a1c8dd9aab1a64ec4df92ec37c5617fa21` |
| image | khoid_outdoor7_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `41f1dfb13c6ef143b689f6a455621ca58006aa99d350ee14d37720e3473b2f14` |
| image | khoid_outdoor7_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `9a2ab64654a1d7a554a9507b41a706dccdb68ac42e6863c795d7d8f3ca86814c` |
| image | khoid_outdoor8_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `55241ab0909b793e74ae10a40aab3d424967f434fdb41d003c229409031c894d` |
| image | khoid_outdoor8_raw_2688x1520_5376 | full | 1920x1080 | 0 | `6b2b703626f695aa480790b348c8513f815b7d754ba0b0a51fd8d9c2db12d75f` |
| image | khoid_outdoor8_raw_2688x1520_5376 | full | 1920x1080 | 1 | `de8963b2228199a45177eebf39ac0cf1a6c3b1450180a036c71ae9a53e974fbe` |
| image | khoid_outdoor8_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `3eff6b30a7ce9d5fd3cd55d1aeb71585e3c5c80142a99f3fedbce9db09331351` |
| image | khoid_outdoor8_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `9a965265d4f3b0e8de45567e2538496234f843c22813676467365ae40d85df31` |
| image | khoid_outdoor8_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `8422aaf5c76a10f1bae437386d727a83b7b9a7b1111e50384da47836865c0b8a` |
| image | khoid_outdoor8_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `8422aaf5c76a10f1bae437386d727a83b7b9a7b1111e50384da47836865c0b8a` |
| image | khoid_outdoor_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `85d0ea88404f3a57063e109d2cec2e31e3285339d03e4c0069da55eed63c32da` |
| image | khoid_outdoor_raw_2688x1520_5376 | full | 1920x1080 | 0 | `a8323efeef1f288eb641a4577aa3c885a6a70e7bf74919d914d312c78fb8d5ff` |
| image | khoid_outdoor_raw_2688x1520_5376 | full | 1920x1080 | 1 | `f33b0e934667c8e8eb0eabfe9e4ccf688f913c920a3b309b8ed6892b98b3d81c` |
| image | khoid_outdoor_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `0c84c206de1c6ddc104ccd1c71e564d8d09774303c066c1fc861270014bdbf76` |
| image | khoid_outdoor_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `26229735c696c72fe96dd856ec42a7484c865047df552c97aab0b7071b3bc5bb` |
| image | khoid_outdoor_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `67aa780c38dff998c96e19dfff2499e603faf2d34063a424f9d1030ed2fba40e` |
| image | khoid_outdoor_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `67aa780c38dff998c96e19dfff2499e603faf2d34063a424f9d1030ed2fba40e` |
| image | khoipd_basement1_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `b2bebfad875b64a2f6a84fafd15b91349c1bc4c60fb4f0b6c9c01426d6190445` |
| image | khoipd_basement1_raw_2688x1520_5376 | full | 1920x1080 | 0 | `53c32a2f80bcf8904272561a70354772464e258ca0902b01d73a0cecac19d80e` |
| image | khoipd_basement1_raw_2688x1520_5376 | full | 1920x1080 | 1 | `1d53c6ccee2d266f79dd1c07a3a0b168826fcf4d17a2001b37ca9fb443c38939` |
| image | khoipd_basement1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `af1ba07cf7b35798abc2e977f70c0942707d080aad586161f7c9c14e3734a50d` |
| image | khoipd_basement1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `0e8c7c5159175e35e588d5677d485687eebe8f874bc7c4f44418683c4a86ad87` |
| image | khoipd_basement1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `40c00c03f8d1512112541d5997d592edb2b3c098244a5ebf5131ffe62cc58771` |
| image | khoipd_basement1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `34605c95c8a2021621f1be3c82afe9f9a606989412a61ec50b53e93a9c90a090` |
| image | khoipd_basement2_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `11506272f1d2e28c596595b9ce55b3139822296b6fc12632e0635eb7d5880b75` |
| image | khoipd_basement2_raw_2688x1520_5376 | full | 1920x1080 | 0 | `25bf416dabead3684a06191fa8e7871425e9b3f31bab772f21b456232022b1af` |
| image | khoipd_basement2_raw_2688x1520_5376 | full | 1920x1080 | 1 | `ab27f907d387578551afe8024c94af4d1befcdf94c059fe9f113c9e661cfa6c7` |
| image | khoipd_basement2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `75fb02054ad0d2dfe4392499f32e38d66b77fa188772256220a1eb4d35acb24b` |
| image | khoipd_basement2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `6a616373e037308a8245c5067fd54894942b24314104ce9ded74c2a86c36923a` |
| image | khoipd_basement2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `a269ef24f1d0e8a80d47f545d083e329c0efbb00bf655d2365a91fd2f7d3fc20` |
| image | khoipd_basement2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `89db1cee9ecdc494f95072fb57f4e0e60ccb7e3b01981ec2494603f056b78a88` |
| image | khoipd_basement_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `2695b8875f6b7cdd4c25d14171e129019e45fd03c7378fc0209a053a103c4a31` |
| image | khoipd_basement_raw_2688x1520_5376 | full | 1920x1080 | 0 | `42de73455853c21a7f783dfb21de55b091de864c09d65ae927b122747448d07a` |
| image | khoipd_basement_raw_2688x1520_5376 | full | 1920x1080 | 1 | `fe95a8a4a7cf6dac54bb18b0c99fe050fed107d06f8b93c9ba39217ba1be1539` |
| image | khoipd_basement_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `fb7bef22a00271e6cad965794d78df7c2280b357ba32b9400e41faec2fad5e37` |
| image | khoipd_basement_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `d96ac8bbe214c61fd0c84318782db568a65158974a34f7f3b8bd4bb38cde9885` |
| image | khoipd_basement_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `f4fce6374a6e0292e04f707958dd49bdecece1a38c66f4cd6279efe485239d56` |
| image | khoipd_basement_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `757439941c3f6aa695cfb128cb6b821be8e76996fcf3d0a133bcd1ce8873cf2c` |
| image | khoipd_indoor1_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `9b04c37c8eb3dbda545a8f6b3f8bb3501d41dfc1f89824eceae656c0a5c900d4` |
| image | khoipd_indoor1_raw_2688x1520_5376 | full | 1920x1080 | 0 | `1a6606c6fbb5868852bde011e5d699d74fd8a7edff2ac4a4921ed2b007128542` |
| image | khoipd_indoor1_raw_2688x1520_5376 | full | 1920x1080 | 1 | `4e8da19f06d609f9b63a8ec975f3918e4e25431935d32fcfad87baed498fd943` |
| image | khoipd_indoor1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `ea25a90d8c21df3348fd4e40ee83f8f3bc94d09bf61a1599504d0900020a9ddb` |
| image | khoipd_indoor1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `6f1243e19605cc79696e5815469fbaf89b35539c2aa583e29eda5ad3cfbf39ea` |
| image | khoipd_indoor1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `93f64d707876b21dca23a7dde1afcd528871826c6ce6365482ff40a63f468309` |
| image | khoipd_indoor1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `175ce52c31daab39c04f33f1d07e16f9f85736bc4c3faf697c3803a7979f2842` |
| image | khoipd_indoor2_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `08085571c5be7663d7e1c1dae9201f276ed7a46e62e10e2525bfad7c4ed319c5` |
| image | khoipd_indoor2_raw_2688x1520_5376 | full | 1920x1080 | 0 | `e3b91ba44eabf6c5bf03457e68d68f805cccbf8dbdbe0eeae207401164f64cfb` |
| image | khoipd_indoor2_raw_2688x1520_5376 | full | 1920x1080 | 1 | `4e788bda1ded827b9698124fd970912170f8d69840c0c174ae5296addb40f47b` |
| image | khoipd_indoor2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `372421d92461a5368ed040fc2f8ee41d947bb678d1e373ace32db0bfac0682ef` |
| image | khoipd_indoor2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `39da9c6b6f49dd43bc3a657c1c90a562752f057f69fa12e35620f9060bd8fc23` |
| image | khoipd_indoor2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `d4f7f0266fbb6f435b408f706771cb0609f5c7de38cebab09b03ad48ad1d1501` |
| image | khoipd_indoor2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `d4f7f0266fbb6f435b408f706771cb0609f5c7de38cebab09b03ad48ad1d1501` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `d2ea3308819f125373d479003ba7e9e644e9e6b8b677e0e4e1aa3fa7e21c5edc` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | full | 1920x1080 | 0 | `3382e196429b332f3e732a365f87508616d45473ffeceea443c42edb873edf7a` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | full | 1920x1080 | 1 | `7238eb538d2cf3d4877448ceaa0b78ddd734cabb6417c8722290c27a06fc5719` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `448baf9bbefd876bbc2139246e56eb743240116a3feb424efa6745e64632c561` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `608a1ae3b09a05edcd7bc688d93294ed7644768050da9b7c24bb5acf638ed579` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `c8abf01c42dc9574bfbf87acc78542b74aa372b0f58e5c3c5066d9efbbcfa5b0` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `e4c5ca6d7170bab04f2b71f40350e06e43abf53761b050f289e3f47663e2136c` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `07e410c51b8fa639e2c096048b6711be2f47e2d59ff1e6f9f3129a92ef397cb5` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | full | 1920x1080 | 0 | `c4717b846371a595cede053b8116ae0b5459fa5a5dc622d7f027eaa5159c4da2` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | full | 1920x1080 | 1 | `8805baf9087719679b083cf2f34de1c435fd9b466fabb31c189def9ed25cd6c5` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `03790671f3650f74d7685fc4170357db7f3dfb1b1d8216331e9c757a328e4054` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `dc4f90b0c4d17ae71c06783c1ed05ca9757013be73d16c89dd83672eaebac35f` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `bdccbbb328122bc98d92760ea22bef6e19d06c78ef52706cf23d32eeea4ef785` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `1397727c055bcb57ada5b953767dc4ddc8df7c85c25590c6bbc7eb20d6a396ec` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `9fe8ffc00c97262886e3c68545dfd9a89d90f1b86aa4f642b344e587292c89ef` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | full | 1920x1080 | 0 | `e85febec7433614d89001d6831032643416c1d2d918a4308c8fdfcd450be5067` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | full | 1920x1080 | 1 | `fe072ef0a8bbfe408e7950309b315ab66082ebad71c3d86f0e42cb1ef45382ee` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `de1290c41656eb5d12bf49a5bf06e02bbd88135206ec0d970c21783979ec9a76` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `00fbe4b5cada7cd847b33ce132da6da2b446b37afeb2a49be334d3bdd58ae24e` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `e7dd152f6bbb9f7e3078ecaa4bc67c7478176cb9551e70a04055f73eb1f5bdbb` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `ef9a91f105bf3d53804a333f2e3d7e7b49e318f7c53f55d13da53ba7ecabf750` |
| image | khoipd_indoor_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `8e756e6ede87a4c873da10fd4b2f6085ad8b29af8b096eff6e4ec4c8fa8e48ce` |
| image | khoipd_indoor_raw_2688x1520_5376 | full | 1920x1080 | 0 | `925dbd2dc07ad4cdaa63ed62c3cdeae7c8f1920e00f2a2e3af3700b86c9e3a01` |
| image | khoipd_indoor_raw_2688x1520_5376 | full | 1920x1080 | 1 | `e87ad637a146263402c9e20e08431a212cd5b2e1e9dd7c5bc59ec6b45422ceda` |
| image | khoipd_indoor_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `4f73951a0386fb26fb3f93a3156db60ec9beb184d7358e67b4a4d282eb9dbe54` |
| image | khoipd_indoor_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `0d35c3994bfe1174c575f065c0f604d3a5dd25c8b49dd47b9e635f99f447aaea` |
| image | khoipd_indoor_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `bcf7a494b56d604d13eb29f5dc3a6c5624031a0b8db21de3368d12cdfe70d285` |
| image | khoipd_indoor_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `5e50c34d9296935c66486a61b471d220c627bd7be16e151e853a76ce1965e17f` |
| image | khoipd_night1_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `8288faa0b1f69df988caa098a2f8e2f18670c88ed50d32037a38429e70f88288` |
| image | khoipd_night1_raw_2688x1520_5376 | full | 1920x1080 | 0 | `aea1ba20fe3632efcbb78f25bf3ac921cfbc11f4fe15c2ad4d6ddeeebf32c96d` |
| image | khoipd_night1_raw_2688x1520_5376 | full | 1920x1080 | 1 | `8882f008be2e81768308da2552496b741a5f4559caca4f87175109a6f6c39651` |
| image | khoipd_night1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `6aca8e7f5aea4028ab6a1f8009e9ee4c93458b4af96850fb2069de391e761e88` |
| image | khoipd_night1_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `f220735a147a8154575b5ba65036e1c5b3e7751eeb3702df46f2023626fd3822` |
| image | khoipd_night1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `cfc2ceeca337914a8984d7a029484310f6d72046cb74a5c27c5533508a0311a3` |
| image | khoipd_night1_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `4703457bc39f41958af867e5da5835eae410eefe41ce7936dd6084bdfdf50e24` |
| image | khoipd_night2_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `5dcef3a3597f625767a2026525673d8f994f36f83ae8085ad3f8c09ce73d1f55` |
| image | khoipd_night2_raw_2688x1520_5376 | full | 1920x1080 | 0 | `46b14e8b1ad220527d2ce8cdc7ae04cc9ca36daaed5b969bfe580b2b773cf38e` |
| image | khoipd_night2_raw_2688x1520_5376 | full | 1920x1080 | 1 | `b70449e35d7128cbeeeb654799acc2c3f785d40be312402d378add69df261632` |
| image | khoipd_night2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `b51fbd440e66a6f501b5c42f4767012983c5e45d5f92472d82bc778770225c5e` |
| image | khoipd_night2_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `5e7da25bd17e55dbc029f9cd683a4d643bde456d77c8c27fe1332884422079cc` |
| image | khoipd_night2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `71851129d7dcb15d17b53bcb0481fb993d509cac8bcd9505711f8a641c01762f` |
| image | khoipd_night2_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `8d823132e7670ef57fc63952e9a8e056aa395e5688bc8db438310f00e124486f` |
| image | khoipd_night3_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `6823a620a7cc219c9746f312be8ea29ba1e5d7913581cf138a45044d4dee7a22` |
| image | khoipd_night3_raw_2688x1520_5376 | full | 1920x1080 | 0 | `70489805035c296552c76f02712919025f8408df6c09a2c6a82bae6902301b44` |
| image | khoipd_night3_raw_2688x1520_5376 | full | 1920x1080 | 1 | `685b4c31a03ac86361adae6aaa3181c10021f1e8eaca2bbe75fea98d25f20250` |
| image | khoipd_night3_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `59b4b80c2fdedb77937344c582b36329456529c6c0a39a734de4df0ab2dd30fe` |
| image | khoipd_night3_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `129ef3129f5e270c508fe98f3ac9a6b150a7fe90c9d0ed8fabdf93a15425455a` |
| image | khoipd_night3_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `7c53c7673e91a092a149458959cf1880f6117089d6203e33648c56bb68a0d52e` |
| image | khoipd_night3_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `35d758752fb303274307f906e7d7490ecd81419d301fe982720dd55078950351` |
| image | khoipd_night4_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `0cda834f7607451c182895f707fc4c055a985122de9dc1687ec17ea86ad87714` |
| image | khoipd_night4_raw_2688x1520_5376 | full | 1920x1080 | 0 | `6e21572cab8762c928440f1c7a1bd978a427bd2d10abef72fd86d57632f1b540` |
| image | khoipd_night4_raw_2688x1520_5376 | full | 1920x1080 | 1 | `f1fc076d935165ad83603abb4ce35c643e304bbb8542124d84e3715282e9e55a` |
| image | khoipd_night4_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `0333c1d538bd61f0369c254b1ee3c83cb07fec5c51d58c111e64d19e7d2214fe` |
| image | khoipd_night4_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `c01e24aa6dd2f4c4d61644942b1c06ad2e246f8a1a00c7eafc6130949bf654eb` |
| image | khoipd_night4_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `6e95bfdf1b3dc6b020a527a6ed0a3ff9792353a810bc700298f96a107385776e` |
| image | khoipd_night4_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `ab4335983274cda1dcd3b75fd26977b111f739f2a442d7df2e43faf8f7315c32` |
| image | khoipd_night5_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `86e0599cebcdc7e2d86bbffaccad220c04ec006a9c46c21a69ba8ffdaabb536c` |
| image | khoipd_night5_raw_2688x1520_5376 | full | 1920x1080 | 0 | `c291ef80517b04e8c88f3165a1fd1646e2b32bd727b786eab4cdd87c8fc2c911` |
| image | khoipd_night5_raw_2688x1520_5376 | full | 1920x1080 | 1 | `4ce9bd9a5c9d8eb8f2c7a75263b02f79910316bb9b23e3977b3db9f9f55b0d33` |
| image | khoipd_night5_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `ad34ed0f55a702bb8881e9750f06d77b6ad425f1cc2906513f60dbe415919fd5` |
| image | khoipd_night5_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `5900ed94184c049cf47d7fb2d1e07a1179a0e06a10d00e4ce62787c68faae594` |
| image | khoipd_night5_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `ad1abd6ffaf7f98d013f6c461e50634499b5a2fbab48b63c2b222241e68c5b6c` |
| image | khoipd_night5_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `528a92619d7bddb255ced2fa17b64cd25cbc0c14db1ba529024453b08035ce66` |
| image | khoipd_night6_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `d086df2947b1730bf94aec64c1215079206e07e6fbb2d486beb52ffa56835b83` |
| image | khoipd_night6_raw_2688x1520_5376 | full | 1920x1080 | 0 | `5926349243cc698d847fb2a931dea15113e2cd41d42a9a0badf5127eceb6540f` |
| image | khoipd_night6_raw_2688x1520_5376 | full | 1920x1080 | 1 | `3711cb12ec96430b6b6a5fa8baba2cc7be8e0a9edca9c573c3d5e8c214e66f1a` |
| image | khoipd_night6_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `a5229f2d8b83a8ffaac33b31934fe299f421a4b7cce57b6544bca50f46e843b0` |
| image | khoipd_night6_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `8a50d0fa73169db776376caa79875e47ec86902575da8d7124747e344b85174e` |
| image | khoipd_night6_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `066d374e8f3ca053aa4055ec4df876093165fb638b926981fc573f380308adee` |
| image | khoipd_night6_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `cf94ae63d9b5a9cbd2e2a6a64b8d044f5cb782ac54a3fb63a481ec33942d74b3` |
| image | khoipd_night_raw_2688x1520_5376 | basic | 2686x1518 | 0 | `30e9f7f12b9a59078ba51cd5e071623a82f2937032323e628f14f99787d74f91` |
| image | khoipd_night_raw_2688x1520_5376 | full | 1920x1080 | 0 | `0fa03c778ab5e0220f87d79e21d6033fed79ea87170a14e1df6482c266c88ee6` |
| image | khoipd_night_raw_2688x1520_5376 | full | 1920x1080 | 1 | `395fc251d991e977d3c5cee4fd8295b4c1d7deffbcde00eca1c6683b654bdec4` |
| image | khoipd_night_raw_2688x1520_5376 | full_vga_709 | 640x480 | 0 | `c973f06ce988d8a26c011210562918c2e1bd457ac1317b5c82a9e2bf8291d267` |
| image | khoipd_night_raw_2688x1520_5376 | full_vga_709 | 640x480 | 1 | `87e07eb096c0863ba6d04d6ae3074390dbcee6f1a85d5c76106169a92325d23a` |
| image | khoipd_night_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 0 | `ffcb71ad6c0d2ae3649e13582980e40138ddd14b55bab9e450fb58b00d19f69f` |
| image | khoipd_night_raw_2688x1520_5376 | gtm_manual_qhd | 960x540 | 1 | `ca344545a3fbdd8db64ab19c83341cce6f1d9ef20769d8c3cdb3234353b4e72a` |
| sequence | sequence of 28 | basic | 2686x1518 | 0 | `b2bebfad875b64a2f6a84fafd15b91349c1bc4c60fb4f0b6c9c01426d6190445` |
| sequence | sequence of 28 | basic | 2686x1518 | 1 | `53c92003fde0ac3f0c6dd11b233a93c63d436baef34d3093b7fc5244840c605f` |
| sequence | sequence of 28 | basic | 2686x1518 | 2 | `cf9b51baa1c9bc2f08a227030879453e205b0a58671fac7177ee25d3189d134a` |
| sequence | sequence of 28 | basic | 2686x1518 | 3 | `3a83b0da070ff19e9bee6782e36f6ed4644a3dbfde1af832d154f64f94b08435` |
| sequence | sequence of 28 | basic | 2686x1518 | 4 | `9a5f04a44f64e4bd09d851fd91d3d2016e94f071695b68f1ab5a319f17120438` |
| sequence | sequence of 28 | basic | 2686x1518 | 5 | `d5f260e4f6beac619eb3d99665bf01ade0d3ec9e787fa518dbd32e2ef24363fb` |
| sequence | sequence of 28 | basic | 2686x1518 | 6 | `fb835ac51f8a07b2bf8cdf381b0acba11e09943a19a1310bd8a0df71ad8b8971` |
| sequence | sequence of 28 | basic | 2686x1518 | 7 | `ba7649140f66e976b6244303843f7a6abe264066c4cf8344d0a9149090580b03` |
| sequence | sequence of 28 | basic | 2686x1518 | 8 | `0194d9f513145fac2b3a840988d4a0bd8969b0c5fd8e1390ae5545f3c0cfdcc0` |
| sequence | sequence of 28 | basic | 2686x1518 | 9 | `0817a5f5b1d2f20ad3a3b49ca55f13d3eef476023492bffbac2d974696137beb` |
| sequence | sequence of 28 | basic | 2686x1518 | 10 | `657efe2f07e36567aa5d6ecf079093f3347164ea47a4451472d65d0572e180c7` |
| sequence | sequence of 28 | basic | 2686x1518 | 11 | `89840b12bd3a32267ac7b649eda51b0812fe27feafb105592106a085c414478e` |
| sequence | sequence of 28 | basic | 2686x1518 | 12 | `0fd0faf4fc26c771642fbc19f3d3bfcd78cf01403dff3819198bd3264c2a0c1b` |
| sequence | sequence of 28 | basic | 2686x1518 | 13 | `9bce23bb6ced1966f5c8b225e56d7d2fe40ca2ef28b94872500beb21f5661e55` |
| sequence | sequence of 28 | basic | 2686x1518 | 14 | `bc466f93e5f72e24527a0a267caa318bda34e53e040f0c3b97967bc9b996c5c2` |
| sequence | sequence of 28 | basic | 2686x1518 | 15 | `09f0a9895c4d89c3b2c169da129c61be2e2d484132483e294251defbd8f58fd3` |
| sequence | sequence of 28 | basic | 2686x1518 | 16 | `620b0ed4ebc68d1ac1dc51a45c112c9e9d82dd8cfd1865abddd12f1352900723` |
| sequence | sequence of 28 | basic | 2686x1518 | 17 | `dc42aa8f21ac4cad3b6b3ad324d8c2b4a86bf82742be65aff6bec1af8cd89fa1` |
| sequence | sequence of 28 | basic | 2686x1518 | 18 | `f92269d3fee61785e8b5a83cc2e32ffdc2aa070493a530f70102cb6d68ed9170` |
| sequence | sequence of 28 | basic | 2686x1518 | 19 | `9d5c1ece6ea095f16a386dd601f0c4f468bb0bc756345188f12c7b731d12f373` |
| sequence | sequence of 28 | basic | 2686x1518 | 20 | `21c0f462b1c11fd0b602b54ec9b1544575baf611a50b6e094bc143cad19366de` |
| sequence | sequence of 28 | basic | 2686x1518 | 21 | `2b10975963d51e28538c1956c972af3a18006244bded87eff2e0b904a29ba78d` |
| sequence | sequence of 28 | basic | 2686x1518 | 22 | `0a38368a1641434afe390645640056d059652ac8897d9d75ba5be352fee15970` |
| sequence | sequence of 28 | basic | 2686x1518 | 23 | `0ea06d4a44e0355f3e97fbc5353a3a07d45e79581085f990e3ae355e32deb5bb` |
| sequence | sequence of 28 | basic | 2686x1518 | 24 | `760579791192cc19c86ceda40684cbe2e09ddbd8b909f08c9c0d6c6d659e7c90` |
| sequence | sequence of 28 | basic | 2686x1518 | 25 | `2922c8f06331963f8bb4a710deeae168ddba41898c7d39254261db7cf38343e8` |
| sequence | sequence of 28 | basic | 2686x1518 | 26 | `63e78c3d0e999bcef9697565d9f25a418f199a44f8415cfddf88dda78724dcac` |
| sequence | sequence of 28 | basic | 2686x1518 | 27 | `0ed889ae6054164e14d38f923e2e34df69f71e9c7fef60e77f38eeadfa36839b` |
| sequence | sequence of 28 | full | 1920x1080 | 0 | `53c32a2f80bcf8904272561a70354772464e258ca0902b01d73a0cecac19d80e` |
| sequence | sequence of 28 | full | 1920x1080 | 1 | `cedac20fd338ec8690330029d11ec69888f488d6c8eef90a8ee09a1e80df1c54` |
| sequence | sequence of 28 | full | 1920x1080 | 2 | `f8b10e8c900a556e3e985bda0546680b6b140321b7ca316e0855f35242c743e8` |
| sequence | sequence of 28 | full | 1920x1080 | 3 | `f6d26f93fe88d726b35e8eaef2673373c4a3bb081c5635da808dd342cc580bfe` |
| sequence | sequence of 28 | full | 1920x1080 | 4 | `afd509b995ecceb5e1046ba85116fe0bd00581acb71bf4aaa73465a00273e8c0` |
| sequence | sequence of 28 | full | 1920x1080 | 5 | `fdcb51478653611622894707174b0161fc289f97e75681644e20795a8aee5036` |
| sequence | sequence of 28 | full | 1920x1080 | 6 | `f73c4da2be6b28eda8949177336a592d61645423b0efd83bf02640e055a02138` |
| sequence | sequence of 28 | full | 1920x1080 | 7 | `8ee4531771e6c46bbd7110039e5ca42d475db9fb9c52173d90e2f99b9b5be891` |
| sequence | sequence of 28 | full | 1920x1080 | 8 | `34c9bc5989275e019210ee67fc289952becebc9474e3f2f9b472374e418dcf96` |
| sequence | sequence of 28 | full | 1920x1080 | 9 | `09e0b3f31598f13b020ae403cea3a4f599ac8e20c82ca3a9a221e71738481984` |
| sequence | sequence of 28 | full | 1920x1080 | 10 | `6787b43d0e690b86afe1412b9b6e5692c2ca62a87c0d27a2d3a707998d6d13cb` |
| sequence | sequence of 28 | full | 1920x1080 | 11 | `5f4952ef89b076b5d46c1dc7d91d2446301548fc6ee62c4327e6590fb17e2eed` |
| sequence | sequence of 28 | full | 1920x1080 | 12 | `ae081fa0fcc26ca1084ac056ddc5bfa445d44c304b88c2650225bcdcc555b4b4` |
| sequence | sequence of 28 | full | 1920x1080 | 13 | `32bb7f944536e6970f3470a22b53a6b2e1b97299230fd8f7f1b19446f06d0d3a` |
| sequence | sequence of 28 | full | 1920x1080 | 14 | `4a61af1414e3e6e6c2b01cda054b7d4fbae2c694e719e8781b9d36cee7eb9985` |
| sequence | sequence of 28 | full | 1920x1080 | 15 | `cffaeb79e0180f84ae3654a2ae67aec5d1fdd4d97da19b02218909bdccdbbb96` |
| sequence | sequence of 28 | full | 1920x1080 | 16 | `94331048cef12bbc58cf3c5e510635e6cce9891623addac365edf184b4ece2a1` |
| sequence | sequence of 28 | full | 1920x1080 | 17 | `a517c7491a3c000bdce04b9d3792e479f420e9783d30eda23ec79e7db6304657` |
| sequence | sequence of 28 | full | 1920x1080 | 18 | `94fcac1845f2c84c4c21c4201b81789bea870a0cbc17230c0b1a4ca843e46cd3` |
| sequence | sequence of 28 | full | 1920x1080 | 19 | `16535b60468ae0be24d94bf1e95fcbf6576d80a1b4ca0b0ce7a1b53df97d7121` |
| sequence | sequence of 28 | full | 1920x1080 | 20 | `19123cccae1550d1424d163e80d614058e7380282f65d4cba3c8d137fd3edda0` |
| sequence | sequence of 28 | full | 1920x1080 | 21 | `ecc472089b2af54077f00a0690cc48e0aecfc3f57e86e5eac7fc68320185bdee` |
| sequence | sequence of 28 | full | 1920x1080 | 22 | `cce67e9ce5595a65569fd385bbe3e49c101ef2adc2e50e4417e8ee8f4b9424ab` |
| sequence | sequence of 28 | full | 1920x1080 | 23 | `33d1600b0b4a66e5d30a41fef7f3f57336006d65c19b97fcc7c92ef5788918b0` |
| sequence | sequence of 28 | full | 1920x1080 | 24 | `3a8e5825374a017816a4ef1f9cd4c124855116caa94c176a70a5d947fc2386c5` |
| sequence | sequence of 28 | full | 1920x1080 | 25 | `b64fcfca7083d21e872ea5872008c9148a6ce3e727bf16348630506e3726cabf` |
| sequence | sequence of 28 | full | 1920x1080 | 26 | `6345456a17f2ee03e771faecbc802332c644010de82a39584ebe2b5cfd9a934b` |
| sequence | sequence of 28 | full | 1920x1080 | 27 | `3035ad32a26bb670cbb4b30a451b9ab811158d3f37aefd79a0ffc8bc021b3702` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 0 | `af1ba07cf7b35798abc2e977f70c0942707d080aad586161f7c9c14e3734a50d` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 1 | `061e43382953589141c6257a1839c8a4f4644be0eccf18e93b8b11716961ff5f` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 2 | `50063e3594123c618405a633b030375f7d8a8acd72fa5ac90532caeac60b45a6` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 3 | `25ea27e4749157b7ac363d421c222d01d6923970c4a0a1c03be3dd1b8027c77a` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 4 | `66a857b5f0b536475404337fb3e07d566e04af1a20aa8c784ae061c1dd560977` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 5 | `39bf0d9464a12fb235a0294204890f3183469db528cc51e4697d058684114752` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 6 | `df261e2f40c0d462a6ea8ff69e74a4cb73a0d548fd0c624897805043efc8ff6b` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 7 | `aac1433e13261c0fa5bb73c861518ee8e64c69cfbd7d6f2340de05dc9efe4a2e` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 8 | `4c22956c7361591fe4459c0cfe36167684dcd8e0e26d131d484fd4e089d1e892` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 9 | `1e46ea44b4fa9bf2ef2ed20f4d8004c4e1d5ef3f2fa6319e45dbc8d37a010f1b` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 10 | `feefaedc018ea39b50f53f70066fa4b1ddd07a7fb02ec7d539ea116ca265210d` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 11 | `162d7aae6101b8e12944fa4c36d2373942b48c9bf7d286dd8aff31db6892516d` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 12 | `606104b90686ac5ce44311eea8176025036512f2cada8bb4e07473c43a5620f0` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 13 | `d70480e326422154f1f3d7d792d21a43c83eac8526d9649aaf398e3cb7b70e7a` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 14 | `65f9a0a3621cb728c1fc5835fab521c113cee1863f54bf5f8999dcfdfeb5cea3` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 15 | `0617d5934aa0b130a535fc9ff31679705d80c846269c891b1e80298e9953c8c6` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 16 | `c9d74c891fb8c08b9c7c107e2d5353cf0eb72bb7066bb8d07bb9a648e93ffd0b` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 17 | `32b2c2a06b635b639650c7f02cc0509e22c728ffdb3021b054740ed8a3d89638` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 18 | `21759c9c0c964121d2c7fbbef84ec1eb720b1784900c89ab16b74bcc2c1776c4` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 19 | `2419f41a7259b5cd18a8a57b79e8b1eb1e3cdcd7028f2743e163ee6f6459abd8` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 20 | `fad32a1a48df429ce9266f326244f6f2495332b0d87539dc5d783a562be2f82d` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 21 | `7472ebd552b1441a0fe477e1811c48a60eeb63bc4a80f76407cf17350d84497f` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 22 | `cd468361546df817ef11b05bc1ec6b2f5c60ff88ee734d269d7db33cccfc271e` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 23 | `0c30d40e83686667157bfd4ec9ca223b237caec506f05777c0310340296b2967` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 24 | `4ce226b69f060753186bb9f5c1b1eec3582c41658f95efbaabce90471372232f` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 25 | `a20772b0ad070b0a921ca2323034b9bc8722870a0074ce34212a40004c1ced20` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 26 | `3c504472b11348c7dc403083623c955ce13f84155d8928446843c0b428fec29c` |
| sequence | sequence of 28 | full_vga_709 | 640x480 | 27 | `cbecfcf49874f55c1a8f273060bd43d0c63f5c399df17a8ca64da3492e52a09a` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 0 | `40c00c03f8d1512112541d5997d592edb2b3c098244a5ebf5131ffe62cc58771` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 1 | `f53caf502dcf97e79ef1b1dc40a75ceecab1666c0afe7a06662054242c4d0370` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 2 | `03e81429ec4d9dec27619626e595e415d2ee77e3c325222dd6ad81b96213ad9a` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 3 | `e74fce6c2ac15eced0905be5b86367b27eaddca69541b625691bd39571d9380d` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 4 | `94ac1353c50ecf1a495c20559ad91504e6ed865c51a8adea1914ef3d09a7ea32` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 5 | `0477f346dbdacaf763c54421db3c612e74e351a4317882d8d7938d5f7e74bbce` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 6 | `7e26a4c612fcee9893a74e31320537f25efcedce5685c6a376b7e3267a428c96` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 7 | `a258ba462a4158cc14c9882b4294e2a83a94626574cfc221bd88d000edfc52b9` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 8 | `6509f0624b20a32cbc81187fba2d3b59ee78b755fe9d10ffed9754763546928d` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 9 | `e754d421192a23a8225a609d46b7dc95ac252739d5f13df22699565219700f6e` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 10 | `bc534c5c939c6f205857c2ba10d751e29afdf20b18e1040b36e780b967721953` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 11 | `31327f4bf6e60b65b828c63477661ff19ca966cd147b809997d5acd8fc17a09f` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 12 | `42ca98c8e45c52783d7e45923dc5929bea0ae75cb3729f94cb74ca800f40e1fb` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 13 | `f8156d0e2f0f26c805323f74572e44211570ffb4a50969e92cf6aad9ef828611` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 14 | `a3e9d2ea7bcd1198bc1093a359a90f485626876c84d3b94f7d5dce00f33126f4` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 15 | `17c2e3ad6f8f02bde40723685ded49ae4bffafa39c4c3af2f22e6949648ff073` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 16 | `33f2dc27ce9f08f869785205c25f11679171793df18c7472e5d4041a04a89ef1` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 17 | `b4fecd75d7bdd3e78ed326cdb4e74033d1c721a51a37cb5c688c98ff3d17b2a1` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 18 | `c4330524228d5c0f3621e4a8d9c84dd6e7ccf944882604b1b2c9c021fbfc5b2d` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 19 | `861867f05c3df747d652ddc124bbcb6f5e9d1121698e9cf18581a203925a61e6` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 20 | `4193be3c8d9a13297704cc1f149808b69c34669ff1bf213f2ee0754100ba55bf` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 21 | `58137c334b7b858e0367b2ee43baffa42e9ea9d9e73cd413411ddcf1ad6fc063` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 22 | `54094fe34eb86265561e49ceaacafe03fd7ebb662eafa58d01e55f741abb41ea` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 23 | `5bd5863e2df9a83a9c6a0b8a6b2af5610e8e40988e251b0a4cfaed2aaabe9b34` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 24 | `ea9f5cadf8853fc3177960328e9eabb0c6241adf6f542978492b07a0d6294fd4` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 25 | `afee1b8b6ae886e56bb485ad823325dec6a5b5e1732b702c6e00a58081850e43` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 26 | `9a9060ffca9a75829a89af792b0e68a3420ffc32be118250192dec2d913071b0` |
| sequence | sequence of 28 | gtm_manual_qhd | 960x540 | 27 | `a3f8afb476ef8d06a52fb1b880fcfbcc3265fc5982e14c1bbf2dbde5907b0dc9` |

Statistics readout SHA-256 (after every frame):

| kind | RAW | preset | SHA-256 |
|---|---|---|---|
| image | khoid_ground_1_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_ground_1_raw_2688x1520_5376 | full | `9d59c7615efdfa7fb12566850da419960ce936883530b2a7c7fe7caf53caf850` |
| image | khoid_ground_1_raw_2688x1520_5376 | full_vga_709 | `26afadc17ab524d07a6067302c40f371f16ec3c5f2c8a6de1bbd25ad54015130` |
| image | khoid_ground_1_raw_2688x1520_5376 | gtm_manual_qhd | `ba1381b1b928eb9f3a0fed41915b7815349921b0545a1d36e8665ee9fda2287f` |
| image | khoid_ground_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_ground_raw_2688x1520_5376 | full | `36c7191a73cd46e58dda3b89a580e7bf65245d575b870a18b3059d75d6eea680` |
| image | khoid_ground_raw_2688x1520_5376 | full_vga_709 | `24cf698a716659d0789cdb2a596fab7b10ad2d6db9015a0d86993282d4e38ea1` |
| image | khoid_ground_raw_2688x1520_5376 | gtm_manual_qhd | `8b4d15f0f95eb1806a65fd31667bf3f59a55520045220dda2cf0852d08399262` |
| image | khoid_indoor_led_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_indoor_led_raw_2688x1520_5376 | full | `b507dd6bfb1ceefb9c161cb9335c7e6201fb6c239c9d5a9bf8ba1bc5a4a9d69a` |
| image | khoid_indoor_led_raw_2688x1520_5376 | full_vga_709 | `35bbddfa32779182b49a0408f37b38eec61eddb2ccb1d85245a50018acd143e1` |
| image | khoid_indoor_led_raw_2688x1520_5376 | gtm_manual_qhd | `f5f6bd821ec8e21c1e4a722247ee6dce2c3ca907202bfe536592b10a3fb06e73` |
| image | khoid_outdoor1_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor1_raw_2688x1520_5376 | full | `62ef2b453760f62503a1eac97db4826f4f7c4dfaad9740308647701ece23062d` |
| image | khoid_outdoor1_raw_2688x1520_5376 | full_vga_709 | `e1c8a342b04c3e8770f78914fd9788b410fe20d163ddcf98db1289b5935e0751` |
| image | khoid_outdoor1_raw_2688x1520_5376 | gtm_manual_qhd | `c8a69706816aa82bcb90536d73980176357bd7bc4b0049ddbdf4e1f147dc4efc` |
| image | khoid_outdoor2_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor2_raw_2688x1520_5376 | full | `ebfd2c86330cbaf6bcd71c72209ab56e7a6dc80f533f84f7e4f5602ad27a5df7` |
| image | khoid_outdoor2_raw_2688x1520_5376 | full_vga_709 | `5832aff6b5a2dd39da065de8228c64f354089dfe013c1a55fd0b7db605d0ab2b` |
| image | khoid_outdoor2_raw_2688x1520_5376 | gtm_manual_qhd | `319e8cb93d8d2dc22f3003977ab46f37ce8ebb06df08b0415a6d456bb17edd15` |
| image | khoid_outdoor3_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor3_raw_2688x1520_5376 | full | `f66e3c4cc757335a32645a56fea9fe701c4cc91b20a3b48bcf9825011d96132d` |
| image | khoid_outdoor3_raw_2688x1520_5376 | full_vga_709 | `caee308345b88d264cf8de7f6fded06a101d2d0c666d0211dd1396e3a5f13b35` |
| image | khoid_outdoor3_raw_2688x1520_5376 | gtm_manual_qhd | `5f4ce288f945503e63145226bf4bb83b46bceccc30094df0969bf8ef734eebfd` |
| image | khoid_outdoor4_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor4_raw_2688x1520_5376 | full | `eb73859f6ebfb6d1c7924966b4e159e039e38b6b2645d03002beab82686c5e23` |
| image | khoid_outdoor4_raw_2688x1520_5376 | full_vga_709 | `54e49f76731a57e31f0e111636563b24f903033d845d1ac7f938dbe134368a26` |
| image | khoid_outdoor4_raw_2688x1520_5376 | gtm_manual_qhd | `ab02efac4a47629b2f21981a94ebe4778a8ca9064d94e92709fa4c294480d842` |
| image | khoid_outdoor5_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor5_raw_2688x1520_5376 | full | `63ff14289ca529df2ac88ea01eab86433a002bc324f5a376e2269ab2ca1bff00` |
| image | khoid_outdoor5_raw_2688x1520_5376 | full_vga_709 | `ea971e90acd1ad27401fff96ceae465b615c9f5a00bc5a49a5306e8cfe42e9ae` |
| image | khoid_outdoor5_raw_2688x1520_5376 | gtm_manual_qhd | `8e82209330b38dc05d526154765d6e01a36757bc2db6751726c8d1086587563a` |
| image | khoid_outdoor6_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor6_raw_2688x1520_5376 | full | `3354a6b68864ccc13be58eb2d8e38777f5dcdc73a18a9e565b1aea20f929866f` |
| image | khoid_outdoor6_raw_2688x1520_5376 | full_vga_709 | `79b3f33ae2f1c6ce68565fba13b84e76df8ab40642fa55e865df4d4a61f517a9` |
| image | khoid_outdoor6_raw_2688x1520_5376 | gtm_manual_qhd | `beb19c9be02175268112e56845da7e010b72948c637d22293749ac52b899cc8d` |
| image | khoid_outdoor7_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor7_raw_2688x1520_5376 | full | `24d18d9c353e536a647eabe5915ccdb0d50237021d6f5a8fe3be5e8b17e5aa7a` |
| image | khoid_outdoor7_raw_2688x1520_5376 | full_vga_709 | `d46d0c7c36c5bc1f3da0812193c74fbf98120af2fd42849140f78125f7a2a12e` |
| image | khoid_outdoor7_raw_2688x1520_5376 | gtm_manual_qhd | `bc0971b59f2c7dafbe3d4706e639a6e496de65e17f849ac583bb17b1c18c1334` |
| image | khoid_outdoor8_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor8_raw_2688x1520_5376 | full | `4f7020beb406ec6393ca5f65cac0c6a2eb1a3e22906d80c1e4680cc2f3e91a47` |
| image | khoid_outdoor8_raw_2688x1520_5376 | full_vga_709 | `7e65d1d5b131dc1ce0f4d6cb94520a549f7279c452869342aaaaaa1ad466826c` |
| image | khoid_outdoor8_raw_2688x1520_5376 | gtm_manual_qhd | `f78ce1d18ee4c2917253ea9ec45a324ea5036fb5592032bdde7b0bf62b48ebdc` |
| image | khoid_outdoor_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoid_outdoor_raw_2688x1520_5376 | full | `22d54ba65e1d1f6f3581a91569f877266f4ba2bc63c6ccde1c71d946a05cc271` |
| image | khoid_outdoor_raw_2688x1520_5376 | full_vga_709 | `24965194b1e79da9d0b422ead9480f71660ed0c15fb3448fa259a22cf66db218` |
| image | khoid_outdoor_raw_2688x1520_5376 | gtm_manual_qhd | `6d2e94f51a60eaa84d066455a5e1e41366f07cbdf6c2f9916dced3a997b0a873` |
| image | khoipd_basement1_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_basement1_raw_2688x1520_5376 | full | `e0844005f3b6c7ca5fc8a28357f7820cd79e72a79953a46df8d2e7a8ad427cdc` |
| image | khoipd_basement1_raw_2688x1520_5376 | full_vga_709 | `5467de0230b98b2fa5b64e4ecdbeaec0b369876dd30df38d091270a56378b5bd` |
| image | khoipd_basement1_raw_2688x1520_5376 | gtm_manual_qhd | `e70c8c0cee15fa6df89de5776a68448ba72171397e895b797edb03375f11b2a8` |
| image | khoipd_basement2_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_basement2_raw_2688x1520_5376 | full | `1f7a1caa69a52bcbf441ad8a5925d22363d9eee4abe48682e5783567df2f8f23` |
| image | khoipd_basement2_raw_2688x1520_5376 | full_vga_709 | `6faa8524705420027df75a7a4613756056715036f229abbd81dab54b56bf9918` |
| image | khoipd_basement2_raw_2688x1520_5376 | gtm_manual_qhd | `259f17dfd4e69664020eccf84eda2c3a79f677e98a7323227d913891e6743211` |
| image | khoipd_basement_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_basement_raw_2688x1520_5376 | full | `2b0d8870c3ffd30561de46d4def1b097a541dc8b1d385edf08c22ff4bb4fac31` |
| image | khoipd_basement_raw_2688x1520_5376 | full_vga_709 | `b176b1b4033ed692e42e0537e8c1df5d3ca060c161885632eb95fd53301ad9e8` |
| image | khoipd_basement_raw_2688x1520_5376 | gtm_manual_qhd | `141b9addbf7c299b490e1f7e17a2ba6e11b2d5cac71e0c0ff8cf13eae0d203a6` |
| image | khoipd_indoor1_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_indoor1_raw_2688x1520_5376 | full | `b41688147ad9646cd2c9c06885a2d7bc99643c3af0247728958ff10ab02fc269` |
| image | khoipd_indoor1_raw_2688x1520_5376 | full_vga_709 | `553a42b6efaa0a1392ad589cd5a3e68728bcd676ae092c305cfd86d40061248d` |
| image | khoipd_indoor1_raw_2688x1520_5376 | gtm_manual_qhd | `03dd8e3de15840e9797326a6b3baad45c7d9752c972fde17eaf4e76333714e3a` |
| image | khoipd_indoor2_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_indoor2_raw_2688x1520_5376 | full | `32708b35e7a4f1774d9b11ad95ff82aee5e0b8c9f056ab336aade611d3a7272d` |
| image | khoipd_indoor2_raw_2688x1520_5376 | full_vga_709 | `d504b5aa3428ad43c65a6ed898322075aad4fad79c7052f449cde100e68b09e6` |
| image | khoipd_indoor2_raw_2688x1520_5376 | gtm_manual_qhd | `f816194d92bf16a945dfde65c950ed815daa7ec149600d9851168535f6c609b2` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | full | `00b4d1bca008f49d05aaa29fa40839d9445bbbfb41e723169837dddc3f639bb1` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | full_vga_709 | `e9c1d08398f55277eafb52cae7fd098dcc8cf1731399663756ee8c618a89bbe6` |
| image | khoipd_indoor_figure1_raw_2688x1520_5376 | gtm_manual_qhd | `c1968ecce4dd6afbb7fb3a75cbe63569365f00b678daa6cd57b11e441e262c30` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | full | `a4f96f825a83378d3529de06aae1ac6f79d0413828ff7f4c26a50cfa2ddb7f66` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | full_vga_709 | `9cadccad3645900ac70e520afb422a23a548e433f73f108afeaaca329ca8ebd2` |
| image | khoipd_indoor_figure2_raw_2688x1520_5376 | gtm_manual_qhd | `3e4aeead296fe5b31a00d4178d6fd1749b42cf2faab6557f2b27e11803a68db8` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | full | `936d8a74a31d23073df0a19a3ac9e42be9829f9ee944c59a4958dc598dc1c9d2` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | full_vga_709 | `a25a498c1cd270838bbab0612b3bea953eae5dd4a2093828ff8445cc7eb469e0` |
| image | khoipd_indoor_figure_raw_2688x1520_5376 | gtm_manual_qhd | `658b2efaf799481bec840a22d831ff8a5cef835a906837414068f76d6ccfdc1d` |
| image | khoipd_indoor_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_indoor_raw_2688x1520_5376 | full | `fc12037e31bd8cb403b4b517bdd530b9060df0a07c477c79f85cfd0a41153f46` |
| image | khoipd_indoor_raw_2688x1520_5376 | full_vga_709 | `3d2c3385982895ec63a2c225ac95db593baaac1992abd683288d54648da9b8cb` |
| image | khoipd_indoor_raw_2688x1520_5376 | gtm_manual_qhd | `e4f75fa82bc53f7522905120f86d74e2475383ac2329cf4dde32a0ff1512060d` |
| image | khoipd_night1_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_night1_raw_2688x1520_5376 | full | `4ea004b0e6b7362112b4b69e62374133c1fb698caed7aa348e0c0f810b6bfb06` |
| image | khoipd_night1_raw_2688x1520_5376 | full_vga_709 | `ca0c59c44cbd625be545d643c12e8d8047343533e0c1c93b5a1058c4ad6213d8` |
| image | khoipd_night1_raw_2688x1520_5376 | gtm_manual_qhd | `5587df0b194527930a3080c1556fe649c1f8e174c3cd0ddaaeca1ad18fa65680` |
| image | khoipd_night2_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_night2_raw_2688x1520_5376 | full | `9ff258b024ba535d66d954027b8573da1c978da75372cb2b985fed1f012607ed` |
| image | khoipd_night2_raw_2688x1520_5376 | full_vga_709 | `3fc2cfc2d5dcdd6055d917f3aa939bac9d96983d90c70408b0a795d0281f8a5e` |
| image | khoipd_night2_raw_2688x1520_5376 | gtm_manual_qhd | `423d480a1b23b59d39e39471cfa4e4f330fbb14a81b05dd45fd218d225a2d759` |
| image | khoipd_night3_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_night3_raw_2688x1520_5376 | full | `1f1638d67e896e4e62de47423891551e3a9c648d153e7316874956cfc0b196c7` |
| image | khoipd_night3_raw_2688x1520_5376 | full_vga_709 | `7548106e1ffc50c8c367d6b32846e1a424d03194b16ddaaaf7c09d984b4befb1` |
| image | khoipd_night3_raw_2688x1520_5376 | gtm_manual_qhd | `37e8b6b6a08759c52f03005530ab1c19e3b2946234a25e474006d32482a44efb` |
| image | khoipd_night4_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_night4_raw_2688x1520_5376 | full | `2f611e08aef176edbfd69af5e5cfb4aa9909e23dc36057e3c040c2ef94326fed` |
| image | khoipd_night4_raw_2688x1520_5376 | full_vga_709 | `8add107ec311981415597cfadac1cd424ac38724a9cf4daf0fc6694fbe1cba61` |
| image | khoipd_night4_raw_2688x1520_5376 | gtm_manual_qhd | `6846b0a5b4d584fd3aac0b1162bae00ce1598ac0007c434d6f4779e7a5e7c6a3` |
| image | khoipd_night5_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_night5_raw_2688x1520_5376 | full | `a3391291ec1a7a0a27557988c1af0fc513207b61a6019f385d22f6a5c7015337` |
| image | khoipd_night5_raw_2688x1520_5376 | full_vga_709 | `b0a990556f203a7ef3c82b7604daf1c057b8301ff0196c1d0f2fb6b89d3f1c32` |
| image | khoipd_night5_raw_2688x1520_5376 | gtm_manual_qhd | `73e61b87606d0252156ef532d066264a435b6dafbfb9cc42c76bb19372bb1d8c` |
| image | khoipd_night6_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_night6_raw_2688x1520_5376 | full | `0f3f3b103129485a57400e477d1afdcb887c2926619cb9ef7ee59a217c6a4f37` |
| image | khoipd_night6_raw_2688x1520_5376 | full_vga_709 | `c04830a2d34267f69bf8d20862e2fc2f67ff03400a247f890c7ae0342a59dbec` |
| image | khoipd_night6_raw_2688x1520_5376 | gtm_manual_qhd | `4aae08420e3f185bec79c2a738676301ef0a7d74a0fef6e92ab866b57470c53e` |
| image | khoipd_night_raw_2688x1520_5376 | basic | `1c1ddfc9985085c3169a5701ecd6c47d8f52b2346e7e53cca377b924e41a6271` |
| image | khoipd_night_raw_2688x1520_5376 | full | `12d297ca1bfb90584cbe9bf2a12886012dead02b74af33d16925308a65099184` |
| image | khoipd_night_raw_2688x1520_5376 | full_vga_709 | `59cf06f0ade604965cdcd7dc1108ea50e2c2ec22fba7c49cb6b5dca139b63521` |
| image | khoipd_night_raw_2688x1520_5376 | gtm_manual_qhd | `89aecb7abb37f8f0b244c46785621509d3746b432d99d715166d0bf54a879d10` |
| sequence | sequence of 28 | basic | `edbb477deed9d8eb648c28bb0fb62f0c58ce4cea90cc945e64242f29c90e3da8` |
| sequence | sequence of 28 | full | `6b29208d7e3751615e1aeb1006161d65d6e6fcbbe7bc917ea0f811e10febb7a1` |
| sequence | sequence of 28 | full_vga_709 | `3b17edd95d182c4a34b9bfb674e241c9d04409f6ca6e086089299898efbcae59` |
| sequence | sequence of 28 | gtm_manual_qhd | `e37f2c667c83877cd654b4a0daf74889d9f0f324a02ee0d08d37f264636c6232` |
