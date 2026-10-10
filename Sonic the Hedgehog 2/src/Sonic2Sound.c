#include "Sound.h"

// Sonic 2's sound ID space and what plays for each id: the Simon Wai prototype's music and effects (SoundIds.h has the ids). The engine's driver knows none of it: it reads this bank.
// (Its sources are SMPS driver 2 files, converted to res/SMPS/converted; the compiler knows their drum names.)


#include "Resource/Music/S2Mus_VsRes_81.h"
#include "Resource/Music/S2Mus_GHz_82.h"
#include "Resource/Music/S2Mus_HPz_83.h"
#include "Resource/Music/S2Mus_OOz_84.h"
#include "Resource/Music/S2Mus_Mz_85.h"
#include "Resource/Music/S2Mus_HTz_86.h"
#include "Resource/Music/S2Mus_NGHz_87.h"
#include "Resource/Music/S2Mus_OOz_88.h"
#include "Resource/Music/S2Mus_CNz_89.h"
#include "Resource/Music/S2Mus_DEz_8A.h"
#include "Resource/Music/S2Mus_DHz_8B.h"
#include "Resource/Music/S2Mus_GHzVs_8C.h"
#include "Resource/Music/S2Mus_SCz_8D.h"
#include "Resource/Music/S2Mus_CPz_8E.h"
#include "Resource/Music/S2Mus_SFz_8F.h"
#include "Resource/Music/S2Mus_HPz_90.h"
#include "Resource/Music/S2Mus_Menu_91.h"
#include "Resource/Music/S2Mus_SS_92.h"
#include "Resource/Music/S2Mus_Boss_93.h"
#include "Resource/Music/S2Mus_DEzFb_94.h"
#include "Resource/Music/S2Mus_EndSq_95.h"
#include "Resource/Music/S2Mus_Super_96.h"
#include "Resource/Music/S2Mus_Invcb_97.h"
#include "Resource/Music/S2Mus_1Up_98.h"
#include "Resource/Music/S2Mus_TScr_99.h"
#include "Resource/Music/S2Mus_LRes_9A.h"
#include "Resource/Music/S2Mus_TGOvr_9B.h"
#include "Resource/Music/S2Mus_Cont_9C.h"
#include "Resource/Music/S2Mus_Emrld_9D.h"
#include "Resource/Music/S2Mus_VsRes_81_json.h"
#include "Resource/Music/S2Mus_GHz_82_json.h"
#include "Resource/Music/S2Mus_HPz_83_json.h"
#include "Resource/Music/S2Mus_OOz_84_json.h"
#include "Resource/Music/S2Mus_Mz_85_json.h"
#include "Resource/Music/S2Mus_HTz_86_json.h"
#include "Resource/Music/S2Mus_NGHz_87_json.h"
#include "Resource/Music/S2Mus_OOz_88_json.h"
#include "Resource/Music/S2Mus_CNz_89_json.h"
#include "Resource/Music/S2Mus_DEz_8A_json.h"
#include "Resource/Music/S2Mus_DHz_8B_json.h"
#include "Resource/Music/S2Mus_GHzVs_8C_json.h"
#include "Resource/Music/S2Mus_SCz_8D_json.h"
#include "Resource/Music/S2Mus_CPz_8E_json.h"
#include "Resource/Music/S2Mus_SFz_8F_json.h"
#include "Resource/Music/S2Mus_HPz_90_json.h"
#include "Resource/Music/S2Mus_Menu_91_json.h"
#include "Resource/Music/S2Mus_SS_92_json.h"
#include "Resource/Music/S2Mus_Boss_93_json.h"
#include "Resource/Music/S2Mus_DEzFb_94_json.h"
#include "Resource/Music/S2Mus_EndSq_95_json.h"
#include "Resource/Music/S2Mus_Super_96_json.h"
#include "Resource/Music/S2Mus_Invcb_97_json.h"
#include "Resource/Music/S2Mus_1Up_98_json.h"
#include "Resource/Music/S2Mus_TScr_99_json.h"
#include "Resource/Music/S2Mus_LRes_9A_json.h"
#include "Resource/Music/S2Mus_TGOvr_9B_json.h"
#include "Resource/Music/S2Mus_Cont_9C_json.h"
#include "Resource/Music/S2Mus_Emrld_9D_json.h"

#include "Resource/Music/S2Sfx_A0.h"
#include "Resource/Music/S2Sfx_A0_json.h"
#include "Resource/Music/S2Sfx_A1.h"
#include "Resource/Music/S2Sfx_A1_json.h"
#include "Resource/Music/S2Sfx_A2.h"
#include "Resource/Music/S2Sfx_A2_json.h"
#include "Resource/Music/S2Sfx_A3.h"
#include "Resource/Music/S2Sfx_A3_json.h"
#include "Resource/Music/S2Sfx_A4.h"
#include "Resource/Music/S2Sfx_A4_json.h"
#include "Resource/Music/S2Sfx_A5.h"
#include "Resource/Music/S2Sfx_A5_json.h"
#include "Resource/Music/S2Sfx_A6.h"
#include "Resource/Music/S2Sfx_A6_json.h"
#include "Resource/Music/S2Sfx_A7.h"
#include "Resource/Music/S2Sfx_A7_json.h"
#include "Resource/Music/S2Sfx_A8.h"
#include "Resource/Music/S2Sfx_A8_json.h"
#include "Resource/Music/S2Sfx_A9.h"
#include "Resource/Music/S2Sfx_A9_json.h"
#include "Resource/Music/S2Sfx_AA.h"
#include "Resource/Music/S2Sfx_AA_json.h"
#include "Resource/Music/S2Sfx_AB.h"
#include "Resource/Music/S2Sfx_AB_json.h"
#include "Resource/Music/S2Sfx_AC.h"
#include "Resource/Music/S2Sfx_AC_json.h"
#include "Resource/Music/S2Sfx_AD.h"
#include "Resource/Music/S2Sfx_AD_json.h"
#include "Resource/Music/S2Sfx_AE.h"
#include "Resource/Music/S2Sfx_AE_json.h"
#include "Resource/Music/S2Sfx_AF.h"
#include "Resource/Music/S2Sfx_AF_json.h"
#include "Resource/Music/S2Sfx_B0.h"
#include "Resource/Music/S2Sfx_B0_json.h"
#include "Resource/Music/S2Sfx_B1.h"
#include "Resource/Music/S2Sfx_B1_json.h"
#include "Resource/Music/S2Sfx_B2.h"
#include "Resource/Music/S2Sfx_B2_json.h"
#include "Resource/Music/S2Sfx_B3.h"
#include "Resource/Music/S2Sfx_B3_json.h"
#include "Resource/Music/S2Sfx_B4.h"
#include "Resource/Music/S2Sfx_B4_json.h"
#include "Resource/Music/S2Sfx_B5.h"
#include "Resource/Music/S2Sfx_B5_json.h"
#include "Resource/Music/S2Sfx_B6.h"
#include "Resource/Music/S2Sfx_B6_json.h"
#include "Resource/Music/S2Sfx_B7.h"
#include "Resource/Music/S2Sfx_B7_json.h"
#include "Resource/Music/S2Sfx_B8.h"
#include "Resource/Music/S2Sfx_B8_json.h"
#include "Resource/Music/S2Sfx_B9.h"
#include "Resource/Music/S2Sfx_B9_json.h"
#include "Resource/Music/S2Sfx_BA.h"
#include "Resource/Music/S2Sfx_BA_json.h"
#include "Resource/Music/S2Sfx_BB.h"
#include "Resource/Music/S2Sfx_BB_json.h"
#include "Resource/Music/S2Sfx_BC.h"
#include "Resource/Music/S2Sfx_BC_json.h"
#include "Resource/Music/S2Sfx_BD.h"
#include "Resource/Music/S2Sfx_BD_json.h"
#include "Resource/Music/S2Sfx_BE.h"
#include "Resource/Music/S2Sfx_BE_json.h"
#include "Resource/Music/S2Sfx_BF.h"
#include "Resource/Music/S2Sfx_BF_json.h"
#include "Resource/Music/S2Sfx_C0.h"
#include "Resource/Music/S2Sfx_C0_json.h"
#include "Resource/Music/S2Sfx_C1.h"
#include "Resource/Music/S2Sfx_C1_json.h"
#include "Resource/Music/S2Sfx_C2.h"
#include "Resource/Music/S2Sfx_C2_json.h"
#include "Resource/Music/S2Sfx_C3.h"
#include "Resource/Music/S2Sfx_C3_json.h"
#include "Resource/Music/S2Sfx_C4.h"
#include "Resource/Music/S2Sfx_C4_json.h"
#include "Resource/Music/S2Sfx_C5.h"
#include "Resource/Music/S2Sfx_C5_json.h"
#include "Resource/Music/S2Sfx_C6.h"
#include "Resource/Music/S2Sfx_C6_json.h"
#include "Resource/Music/S2Sfx_C7.h"
#include "Resource/Music/S2Sfx_C7_json.h"
#include "Resource/Music/S2Sfx_C8.h"
#include "Resource/Music/S2Sfx_C8_json.h"
#include "Resource/Music/S2Sfx_C9.h"
#include "Resource/Music/S2Sfx_C9_json.h"
#include "Resource/Music/S2Sfx_CA.h"
#include "Resource/Music/S2Sfx_CA_json.h"
#include "Resource/Music/S2Sfx_CB.h"
#include "Resource/Music/S2Sfx_CB_json.h"
#include "Resource/Music/S2Sfx_CC.h"
#include "Resource/Music/S2Sfx_CC_json.h"
#include "Resource/Music/S2Sfx_CD.h"
#include "Resource/Music/S2Sfx_CD_json.h"
#include "Resource/Music/S2Sfx_CE.h"
#include "Resource/Music/S2Sfx_CE_json.h"
#include "Resource/Music/S2Sfx_CF.h"
#include "Resource/Music/S2Sfx_CF_json.h"

#include "Resource/Music/S2Sfx_D0.h"
#include "Resource/Music/S2Sfx_D1.h"
#include "Resource/Music/S2Sfx_D2.h"
#include "Resource/Music/S2Sfx_D3.h"
#include "Resource/Music/S2Sfx_D4.h"
#include "Resource/Music/S2Sfx_D5.h"
#include "Resource/Music/S2Sfx_D6.h"
#include "Resource/Music/S2Sfx_D7.h"
#include "Resource/Music/S2Sfx_D8.h"
#include "Resource/Music/S2Sfx_D9.h"
#include "Resource/Music/S2Sfx_DA.h"
#include "Resource/Music/S2Sfx_DB.h"
#include "Resource/Music/S2Sfx_DC.h"
#include "Resource/Music/S2Sfx_DD.h"
#include "Resource/Music/S2Sfx_DE.h"
#include "Resource/Music/S2Sfx_DF.h"
#include "Resource/Music/S2Sfx_E0.h"
#include "Resource/Music/S2Sfx_D0_json.h"
#include "Resource/Music/S2Sfx_D1_json.h"
#include "Resource/Music/S2Sfx_D2_json.h"
#include "Resource/Music/S2Sfx_D3_json.h"
#include "Resource/Music/S2Sfx_D4_json.h"
#include "Resource/Music/S2Sfx_D5_json.h"
#include "Resource/Music/S2Sfx_D6_json.h"
#include "Resource/Music/S2Sfx_D7_json.h"
#include "Resource/Music/S2Sfx_D8_json.h"
#include "Resource/Music/S2Sfx_D9_json.h"
#include "Resource/Music/S2Sfx_DA_json.h"
#include "Resource/Music/S2Sfx_DB_json.h"
#include "Resource/Music/S2Sfx_DC_json.h"
#include "Resource/Music/S2Sfx_DD_json.h"
#include "Resource/Music/S2Sfx_DE_json.h"
#include "Resource/Music/S2Sfx_DF_json.h"
#include "Resource/Music/S2Sfx_E0_json.h"

static const uint8_t *const sound_table[0x100] = {
    [mus_VsResults] = S2Mus_VsRes_81,
    [mus_EHZ] = S2Mus_GHz_82,
    [mus_MCZ2P] = S2Mus_HPz_83,
    [mus_OOZ] = S2Mus_OOz_84,
    [mus_MTZ] = S2Mus_Mz_85,
    [mus_HTZ] = S2Mus_HTz_86,
    [mus_ARZ] = S2Mus_NGHz_87,
    [mus_CNZ2P] = S2Mus_OOz_88,
    [mus_CNZ] = S2Mus_CNz_89,
    [mus_DEZ] = S2Mus_DEz_8A,
    [mus_MCZ] = S2Mus_DHz_8B,
    [mus_EHZ2P] = S2Mus_GHzVs_8C,
    [mus_SCZ] = S2Mus_SCz_8D,
    [mus_CPZ] = S2Mus_CPz_8E,
    [mus_WFZ] = S2Mus_SFz_8F,
    [mus_HPZ] = S2Mus_HPz_90,
    [mus_Menu] = S2Mus_Menu_91,
    [mus_SpecStg] = S2Mus_SS_92,
    [mus_Boss] = S2Mus_Boss_93,
    [mus_FinalBoss] = S2Mus_DEzFb_94,
    [mus_Ending] = S2Mus_EndSq_95,
    [mus_Super] = S2Mus_Super_96,
    [mus_Invinc] = S2Mus_Invcb_97,
    [mus_ExtraLife] = S2Mus_1Up_98,
    [mus_Title] = S2Mus_TScr_99,
    [mus_ActClear] = S2Mus_LRes_9A,
    [mus_GameOver] = S2Mus_TGOvr_9B,
    [mus_Continue] = S2Mus_Cont_9C,
    [mus_Emerald] = S2Mus_Emrld_9D,

    [sfx_Jump] = S2Sfx_A0,
    [sfx_Lamppost] = S2Sfx_A1,
    [sfx_Unk_A2] = S2Sfx_A2,
    [sfx_Death] = S2Sfx_A3,
    [sfx_Skid] = S2Sfx_A4,
    [sfx_Unk_A5] = S2Sfx_A5,
    [sfx_HitSpikes] = S2Sfx_A6,
    [sfx_Push] = S2Sfx_A7,
    [sfx_SSGoal] = S2Sfx_A8,
    [sfx_SSItem] = S2Sfx_A9,
    [sfx_Splash] = S2Sfx_AA,
    [sfx_Unk_AB] = S2Sfx_AB,
    [sfx_HitBoss] = S2Sfx_AC,
    [sfx_Bubble] = S2Sfx_AD,
    [sfx_Fireball] = S2Sfx_AE,
    [sfx_Shield] = S2Sfx_AF,
    [sfx_Saw] = S2Sfx_B0,
    [sfx_Electric] = S2Sfx_B1,
    [sfx_Drown] = S2Sfx_B2,
    [sfx_Flamethrower] = S2Sfx_B3,
    [sfx_Bumper] = S2Sfx_B4,
    [sfx_Ring] = S2Sfx_B5,
    [sfx_SpikesMove] = S2Sfx_B6,
    [sfx_Rumbling] = S2Sfx_B7,
    [sfx_Unk_B8] = S2Sfx_B8,
    [sfx_Collapse] = S2Sfx_B9,
    [sfx_SSGlass] = S2Sfx_BA,
    [sfx_Door] = S2Sfx_BB,
    [sfx_Teleport] = S2Sfx_BC,
    [sfx_ChainStomp] = S2Sfx_BD,
    [sfx_Roll] = S2Sfx_BE,
    [sfx_Continue] = S2Sfx_BF,
    [sfx_Basaran] = S2Sfx_C0,
    [sfx_BreakItem] = S2Sfx_C1,
    [sfx_Warning] = S2Sfx_C2,
    [sfx_GiantRing] = S2Sfx_C3,
    [sfx_Bomb] = S2Sfx_C4,
    [sfx_Cash] = S2Sfx_C5,
    [sfx_RingLoss] = S2Sfx_C6,
    [sfx_ChainRise] = S2Sfx_C7,
    [sfx_Burning] = S2Sfx_C8,
    [sfx_Bonus] = S2Sfx_C9,
    [sfx_EnterSS] = S2Sfx_CA,
    [sfx_WallSmash] = S2Sfx_CB,
    [sfx_Spring] = S2Sfx_CC,
    [sfx_Switch] = S2Sfx_CD,
    [sfx_RingLeft] = S2Sfx_CE,
    [sfx_Signpost] = S2Sfx_CF,
    [sfx_CNZBossZap] = S2Sfx_D0,
    [sfx_Unk_D1] = S2Sfx_D1,
    [sfx_Unk_D2] = S2Sfx_D2,
    [sfx_Signpost2P] = S2Sfx_D3,
    [sfx_OOZLidPop] = S2Sfx_D4,
    [sfx_SlidingSpike] = S2Sfx_D5,
    [sfx_CNZElevator] = S2Sfx_D6,
    [sfx_PlatformKnock] = S2Sfx_D7,
    [sfx_BonusBumper] = S2Sfx_D8,
    [sfx_LargeBumper] = S2Sfx_D9,
    [sfx_Gloop] = S2Sfx_DA,
    [sfx_PreArrowFiring] = S2Sfx_DB,
    [sfx_Fire] = S2Sfx_DC,
    [sfx_ArrowStick] = S2Sfx_DD,
    [sfx_Helicopter] = S2Sfx_DE,
    [sfx_HelicopterDup] = S2Sfx_DE,
    [sfx_SuperTransform] = S2Sfx_DF,
    [sfx_SpindashRev] = S2Sfx_E0,
};

static const char *const sound_table_json[0x100] = {
    [mus_VsResults] = S2Mus_VsRes_81_json,
    [mus_EHZ] = S2Mus_GHz_82_json,
    [mus_MCZ2P] = S2Mus_HPz_83_json,
    [mus_OOZ] = S2Mus_OOz_84_json,
    [mus_MTZ] = S2Mus_Mz_85_json,
    [mus_HTZ] = S2Mus_HTz_86_json,
    [mus_ARZ] = S2Mus_NGHz_87_json,
    [mus_CNZ2P] = S2Mus_OOz_88_json,
    [mus_CNZ] = S2Mus_CNz_89_json,
    [mus_DEZ] = S2Mus_DEz_8A_json,
    [mus_MCZ] = S2Mus_DHz_8B_json,
    [mus_EHZ2P] = S2Mus_GHzVs_8C_json,
    [mus_SCZ] = S2Mus_SCz_8D_json,
    [mus_CPZ] = S2Mus_CPz_8E_json,
    [mus_WFZ] = S2Mus_SFz_8F_json,
    [mus_HPZ] = S2Mus_HPz_90_json,
    [mus_Menu] = S2Mus_Menu_91_json,
    [mus_SpecStg] = S2Mus_SS_92_json,
    [mus_Boss] = S2Mus_Boss_93_json,
    [mus_FinalBoss] = S2Mus_DEzFb_94_json,
    [mus_Ending] = S2Mus_EndSq_95_json,
    [mus_Super] = S2Mus_Super_96_json,
    [mus_Invinc] = S2Mus_Invcb_97_json,
    [mus_ExtraLife] = S2Mus_1Up_98_json,
    [mus_Title] = S2Mus_TScr_99_json,
    [mus_ActClear] = S2Mus_LRes_9A_json,
    [mus_GameOver] = S2Mus_TGOvr_9B_json,
    [mus_Continue] = S2Mus_Cont_9C_json,
    [mus_Emerald] = S2Mus_Emrld_9D_json,

    [sfx_Jump] = S2Sfx_A0_json,
    [sfx_Lamppost] = S2Sfx_A1_json,
    [sfx_Unk_A2] = S2Sfx_A2_json,
    [sfx_Death] = S2Sfx_A3_json,
    [sfx_Skid] = S2Sfx_A4_json,
    [sfx_Unk_A5] = S2Sfx_A5_json,
    [sfx_HitSpikes] = S2Sfx_A6_json,
    [sfx_Push] = S2Sfx_A7_json,
    [sfx_SSGoal] = S2Sfx_A8_json,
    [sfx_SSItem] = S2Sfx_A9_json,
    [sfx_Splash] = S2Sfx_AA_json,
    [sfx_Unk_AB] = S2Sfx_AB_json,
    [sfx_HitBoss] = S2Sfx_AC_json,
    [sfx_Bubble] = S2Sfx_AD_json,
    [sfx_Fireball] = S2Sfx_AE_json,
    [sfx_Shield] = S2Sfx_AF_json,
    [sfx_Saw] = S2Sfx_B0_json,
    [sfx_Electric] = S2Sfx_B1_json,
    [sfx_Drown] = S2Sfx_B2_json,
    [sfx_Flamethrower] = S2Sfx_B3_json,
    [sfx_Bumper] = S2Sfx_B4_json,
    [sfx_Ring] = S2Sfx_B5_json,
    [sfx_SpikesMove] = S2Sfx_B6_json,
    [sfx_Rumbling] = S2Sfx_B7_json,
    [sfx_Unk_B8] = S2Sfx_B8_json,
    [sfx_Collapse] = S2Sfx_B9_json,
    [sfx_SSGlass] = S2Sfx_BA_json,
    [sfx_Door] = S2Sfx_BB_json,
    [sfx_Teleport] = S2Sfx_BC_json,
    [sfx_ChainStomp] = S2Sfx_BD_json,
    [sfx_Roll] = S2Sfx_BE_json,
    [sfx_Continue] = S2Sfx_BF_json,
    [sfx_Basaran] = S2Sfx_C0_json,
    [sfx_BreakItem] = S2Sfx_C1_json,
    [sfx_Warning] = S2Sfx_C2_json,
    [sfx_GiantRing] = S2Sfx_C3_json,
    [sfx_Bomb] = S2Sfx_C4_json,
    [sfx_Cash] = S2Sfx_C5_json,
    [sfx_RingLoss] = S2Sfx_C6_json,
    [sfx_ChainRise] = S2Sfx_C7_json,
    [sfx_Burning] = S2Sfx_C8_json,
    [sfx_Bonus] = S2Sfx_C9_json,
    [sfx_EnterSS] = S2Sfx_CA_json,
    [sfx_WallSmash] = S2Sfx_CB_json,
    [sfx_Spring] = S2Sfx_CC_json,
    [sfx_Switch] = S2Sfx_CD_json,
    [sfx_RingLeft] = S2Sfx_CE_json,
    [sfx_Signpost] = S2Sfx_CF_json,
    [sfx_CNZBossZap] = S2Sfx_D0_json,
    [sfx_Unk_D1] = S2Sfx_D1_json,
    [sfx_Unk_D2] = S2Sfx_D2_json,
    [sfx_Signpost2P] = S2Sfx_D3_json,
    [sfx_OOZLidPop] = S2Sfx_D4_json,
    [sfx_SlidingSpike] = S2Sfx_D5_json,
    [sfx_CNZElevator] = S2Sfx_D6_json,
    [sfx_PlatformKnock] = S2Sfx_D7_json,
    [sfx_BonusBumper] = S2Sfx_D8_json,
    [sfx_LargeBumper] = S2Sfx_D9_json,
    [sfx_Gloop] = S2Sfx_DA_json,
    [sfx_PreArrowFiring] = S2Sfx_DB_json,
    [sfx_Fire] = S2Sfx_DC_json,
    [sfx_ArrowStick] = S2Sfx_DD_json,
    [sfx_Helicopter] = S2Sfx_DE_json,
    [sfx_HelicopterDup] = S2Sfx_DE_json,
    [sfx_SuperTransform] = S2Sfx_DF_json,
    [sfx_SpindashRev] = S2Sfx_E0_json,
};

// Every song runs on driver version 1's coordination flags, converted from the sources' own. The songs the final game keeps compressed (every one but the 1-up, game over and emerald jingles) have Sonic 2's tempo
// (written for it; Sonic 1's tempo byte cannot play the slow ones); those three keep Sonic 1's (converted with tools/import_s2_music.pl --s1-tempo), and so do the effects (they have no tempo)
static const uint8_t sound_table_driver_ver[0x100] = {
    [mus_VsResults] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_EHZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_MCZ2P] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_OOZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_MTZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_HTZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_ARZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_CNZ2P] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_CNZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_DEZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_MCZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_EHZ2P] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_SCZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_CPZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_WFZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_HPZ] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_Menu] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_SpecStg] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_Boss] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_FinalBoss] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_Ending] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_Super] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_Invinc] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_Title] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_ActClear] = SOUND_DRIVER_VERSION_2_TEMPO,
    [mus_Continue] = SOUND_DRIVER_VERSION_2_TEMPO,
};

// The prototype's SndPriorities, for each effect from $A0 on (higher wins)
static const uint8_t sound_priorities[SOUND_ID_LAST - sfx_Jump + 1] = {
    0x80, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x68, // $A0
    0x70, 0x70, 0x70, 0x60, 0x70, 0x70, 0x60, 0x70, 0x60, 0x70, 0x70, // $AB
    0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x7F, 0x60, // $B6
    0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, // $C1
    0x70, 0x70, 0x70, 0x70, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, // $CC
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x90, 0x90, // $D7
};

// The tempo each song takes under speed shoes: the final game's zSpedUpTempoTable, by song (Sonic 2 tempos, as the songs' own; the 1-up, game over and emerald jingles' are converted to Sonic 1's: $05, $12, $06)
static const uint8_t speedup_index[31] = {
    0x68, 0xBE, 0xFF, 0xF0, 0xFF, 0xDE, 0xFF, 0xDD,
    0x68, 0x80, 0xD6, 0x7B, 0x7B, 0xFF, 0xA8, 0xFF,
    0x87, 0xFF, 0xFF, 0xC9, 0x97, 0xFF, 0xFF, 0x05,
    0xCD, 0xAA, 0x12, 0xDB, 0x06, 0xF0, 0x80,
};

// The SMPS Inspector's list of sounds (Tools menu): music first, then effects.
#define INSPECT_ENTRY(id, is_music, name) {(uint8_t)(id), (is_music), (name)}
static const SoundInspectEntry inspect_entries[] = {
    INSPECT_ENTRY(mus_VsResults, 1, "2P Results"),
    INSPECT_ENTRY(mus_EHZ, 1, "Emerald Hill"),
    INSPECT_ENTRY(mus_MCZ2P, 1, "Mystic Cave 2P"),
    INSPECT_ENTRY(mus_OOZ, 1, "Oil Ocean"),
    INSPECT_ENTRY(mus_MTZ, 1, "Metropolis"),
    INSPECT_ENTRY(mus_HTZ, 1, "Hill Top"),
    INSPECT_ENTRY(mus_ARZ, 1, "Neo Green Hill"),
    INSPECT_ENTRY(mus_CNZ2P, 1, "Casino Night 2P"),
    INSPECT_ENTRY(mus_CNZ, 1, "Casino Night"),
    INSPECT_ENTRY(mus_DEZ, 1, "Death Egg"),
    INSPECT_ENTRY(mus_MCZ, 1, "Mystic Cave (Dust Hill)"),
    INSPECT_ENTRY(mus_EHZ2P, 1, "Emerald Hill 2P"),
    INSPECT_ENTRY(mus_SCZ, 1, "Sky Chase"),
    INSPECT_ENTRY(mus_CPZ, 1, "Chemical Plant"),
    INSPECT_ENTRY(mus_WFZ, 1, "Wing Fortress"),
    INSPECT_ENTRY(mus_HPZ, 1, "Hidden Palace"),
    INSPECT_ENTRY(mus_Menu, 1, "Options / Level Select"),
    INSPECT_ENTRY(mus_SpecStg, 1, "Special Stage"),
    INSPECT_ENTRY(mus_Boss, 1, "Boss"),
    INSPECT_ENTRY(mus_FinalBoss, 1, "Death Egg Final Boss"),
    INSPECT_ENTRY(mus_Ending, 1, "Ending"),
    INSPECT_ENTRY(mus_Super, 1, "Super Sonic"),
    INSPECT_ENTRY(mus_Invinc, 1, "Invincibility"),
    INSPECT_ENTRY(mus_ExtraLife, 1, "Extra Life"),
    INSPECT_ENTRY(mus_Title, 1, "Title Screen"),
    INSPECT_ENTRY(mus_ActClear, 1, "Act Clear"),
    INSPECT_ENTRY(mus_GameOver, 1, "Game Over"),
    INSPECT_ENTRY(mus_Continue, 1, "Continue"),
    INSPECT_ENTRY(mus_Emerald, 1, "Got an Emerald"),
    INSPECT_ENTRY(sfx_Jump, 0, "Jump"),
    INSPECT_ENTRY(sfx_Lamppost, 0, "Lamppost"),
    INSPECT_ENTRY(sfx_Unk_A2, 0, "Unused A2"),
    INSPECT_ENTRY(sfx_Death, 0, "Death"),
    INSPECT_ENTRY(sfx_Skid, 0, "Skid"),
    INSPECT_ENTRY(sfx_Unk_A5, 0, "Unused A5"),
    INSPECT_ENTRY(sfx_HitSpikes, 0, "Hit Spikes"),
    INSPECT_ENTRY(sfx_Push, 0, "Push Block"),
    INSPECT_ENTRY(sfx_SSGoal, 0, "SS Goal"),
    INSPECT_ENTRY(sfx_SSItem, 0, "SS Item"),
    INSPECT_ENTRY(sfx_Splash, 0, "Splash"),
    INSPECT_ENTRY(sfx_Unk_AB, 0, "Unused AB"),
    INSPECT_ENTRY(sfx_HitBoss, 0, "Hit Boss"),
    INSPECT_ENTRY(sfx_Bubble, 0, "Get Bubble"),
    INSPECT_ENTRY(sfx_Fireball, 0, "Fireball"),
    INSPECT_ENTRY(sfx_Shield, 0, "Shield"),
    INSPECT_ENTRY(sfx_Saw, 0, "Saw"),
    INSPECT_ENTRY(sfx_Electric, 0, "Electric"),
    INSPECT_ENTRY(sfx_Drown, 0, "Drown Death"),
    INSPECT_ENTRY(sfx_Flamethrower, 0, "Flamethrower"),
    INSPECT_ENTRY(sfx_Bumper, 0, "Bumper"),
    INSPECT_ENTRY(sfx_Ring, 0, "Ring"),
    INSPECT_ENTRY(sfx_SpikesMove, 0, "Spikes Move"),
    INSPECT_ENTRY(sfx_Rumbling, 0, "Rumbling"),
    INSPECT_ENTRY(sfx_Unk_B8, 0, "Unused B8"),
    INSPECT_ENTRY(sfx_Collapse, 0, "Collapse"),
    INSPECT_ENTRY(sfx_SSGlass, 0, "SS Glass"),
    INSPECT_ENTRY(sfx_Door, 0, "Door"),
    INSPECT_ENTRY(sfx_Teleport, 0, "Teleport / Spin Dash Release"),
    INSPECT_ENTRY(sfx_ChainStomp, 0, "Chain Stomp"),
    INSPECT_ENTRY(sfx_Roll, 0, "Roll"),
    INSPECT_ENTRY(sfx_Continue, 0, "Get Continue"),
    INSPECT_ENTRY(sfx_Basaran, 0, "Basaran Flap"),
    INSPECT_ENTRY(sfx_BreakItem, 0, "Break Item"),
    INSPECT_ENTRY(sfx_Warning, 0, "Drown Warning"),
    INSPECT_ENTRY(sfx_GiantRing, 0, "Giant Ring"),
    INSPECT_ENTRY(sfx_Bomb, 0, "Bomb"),
    INSPECT_ENTRY(sfx_Cash, 0, "Cash Register"),
    INSPECT_ENTRY(sfx_RingLoss, 0, "Ring Loss"),
    INSPECT_ENTRY(sfx_ChainRise, 0, "Chain Rising"),
    INSPECT_ENTRY(sfx_Burning, 0, "Burning"),
    INSPECT_ENTRY(sfx_Bonus, 0, "Hidden Bonus"),
    INSPECT_ENTRY(sfx_EnterSS, 0, "Enter Special Stage"),
    INSPECT_ENTRY(sfx_WallSmash, 0, "Wall Smash"),
    INSPECT_ENTRY(sfx_Spring, 0, "Spring"),
    INSPECT_ENTRY(sfx_Switch, 0, "Switch"),
    INSPECT_ENTRY(sfx_RingLeft, 0, "Ring (left speaker)"),
    INSPECT_ENTRY(sfx_Signpost, 0, "Signpost"),
    INSPECT_ENTRY(sfx_CNZBossZap, 0, "CNZ Boss Zap"),
    INSPECT_ENTRY(sfx_Unk_D1, 0, "Unknown (Unused)"),
    INSPECT_ENTRY(sfx_Unk_D2, 0, "Unknown (Unused)"),
    INSPECT_ENTRY(sfx_Signpost2P, 0, "Signpost 2P"),
    INSPECT_ENTRY(sfx_OOZLidPop, 0, "OOZ Lid Pop"),
    INSPECT_ENTRY(sfx_SlidingSpike, 0, "Sliding Spike"),
    INSPECT_ENTRY(sfx_CNZElevator, 0, "CNZ Elevator"),
    INSPECT_ENTRY(sfx_PlatformKnock, 0, "Platform Knock"),
    INSPECT_ENTRY(sfx_BonusBumper, 0, "Bonus Bumper"),
    INSPECT_ENTRY(sfx_LargeBumper, 0, "Large Bumper"),
    INSPECT_ENTRY(sfx_Gloop, 0, "Gloop"),
    INSPECT_ENTRY(sfx_PreArrowFiring, 0, "Pre-Arrow Firing"),
    INSPECT_ENTRY(sfx_Fire, 0, "Fire"),
    INSPECT_ENTRY(sfx_ArrowStick, 0, "Arrow Stick"),
    INSPECT_ENTRY(sfx_Helicopter, 0, "Helicopter"),
    INSPECT_ENTRY(sfx_HelicopterDup, 0, "Helicopter"),
    INSPECT_ENTRY(sfx_SuperTransform, 0, "Super Transform"),
    INSPECT_ENTRY(sfx_SpindashRev, 0, "Spin Dash Rev"),
};

const SoundBank game_sound_bank = {
    .songs = sound_table,
    .songs_json = sound_table_json,
    .driver_version = sound_table_driver_ver,
    .music_first = mus_VsResults,
    .music_last = mus_Emerald,
    .sfx_first = sfx_Jump,
    .special_last = sfx_SpindashRev,
    .command_first = bgm_Fade,
    .command_last = bgm_Stop,
    .sfx_priorities = sound_priorities,
    .command_fade = bgm_Fade,
    .command_sega = sfx_Sega,
    .command_speedup = bgm_Speedup,
    .command_slowdown = bgm_Slowdown,
    .command_stop = bgm_Stop,
    .one_up_music = mus_ExtraLife,
    .spindash_rev_sfx = sfx_SpindashRev,
    .ring_sfx = sfx_Ring,
    .ring_left_sfx = sfx_RingLeft,
    .push_sfx = sfx_Push,
    .speedup_tempo = speedup_index,
    .speedup_tempo_count = sizeof(speedup_index) / sizeof(speedup_index[0]),
    .inspect_entries = inspect_entries,
    .inspect_entry_count = (int)(sizeof(inspect_entries) / sizeof(inspect_entries[0])),
};
