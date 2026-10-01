using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using SonicRetro.SonLVL.API;

namespace S1ObjectDefinitions.SBZ
{
	// Object 6F - spinning platforms on a conveyor belt (SBZ1).
	//
	// Subtypes $80-$85 are "spawners": in the game a spawner turns into a whole
	// group of 8 platforms spread around its belt loop, using the placement
	// tables objpos/platforms/sbz1pf1.bin ... sbz1pf6.bin (ObjPosSBZPlatform_Index).
	// So the editor draws the whole group, not a single platform -- same scheme
	// as the LZ conveyor (Object 63).
	class PlatformConveyor : ObjectDefinition
	{
		private Sprite platformimg;

		// Start position (X, Y pairs) of every platform in each spawner group.
		// Absolute level coordinates; GetSprite draws them relative to the spawner.
		private static readonly int[][] groups = new int[][] {
			// Group 0 (SBZ1 platforms 1)
			new int[] {
				0x0E14, 0x0370,
				0x0E5A, 0x034D,
				0x0EA0, 0x032A,
				0x0EE7, 0x0307,
				0x0EEF, 0x0340,
				0x0EA9, 0x0363,
				0x0E63, 0x0386,
				0x0E1C, 0x03A9,
			},
			// Group 1 (SBZ1 platforms 2)
			new int[] {
				0x0F14, 0x02E0,
				0x0F5A, 0x02BD,
				0x0FA0, 0x029A,
				0x0FE7, 0x0277,
				0x0FEF, 0x02B0,
				0x0FA9, 0x02D3,
				0x0F63, 0x02F6,
				0x0F1C, 0x0319,
			},
			// Group 2 (SBZ1 platforms 3)
			new int[] {
				0x1014, 0x0270,
				0x105A, 0x024D,
				0x10A0, 0x022A,
				0x10E7, 0x0207,
				0x10EF, 0x0240,
				0x10A9, 0x0263,
				0x1063, 0x0286,
				0x101C, 0x02A9,
			},
			// Group 3 (SBZ1 platforms 4)
			new int[] {
				0x0F14, 0x0570,
				0x0F5A, 0x054D,
				0x0FA0, 0x052A,
				0x0FE7, 0x0507,
				0x0FEF, 0x0540,
				0x0FA9, 0x0563,
				0x0F63, 0x0586,
				0x0F1C, 0x05A9,
			},
			// Group 4 (SBZ1 platforms 5)
			new int[] {
				0x1B14, 0x0670,
				0x1B5A, 0x064D,
				0x1BA0, 0x062A,
				0x1BE7, 0x0607,
				0x1BEF, 0x0640,
				0x1BA9, 0x0663,
				0x1B63, 0x0686,
				0x1B1C, 0x06A9,
			},
			// Group 5 (SBZ1 platforms 6)
			new int[] {
				0x1C14, 0x05E0,
				0x1C5A, 0x05BD,
				0x1CA0, 0x059A,
				0x1CE7, 0x0577,
				0x1CEF, 0x05B0,
				0x1CA9, 0x05D3,
				0x1C63, 0x05F6,
				0x1C1C, 0x0619,
			}		};

		public override void Init(ObjectData data)
		{
			byte[] artfile = ObjectHelper.OpenArtFile("../res/Art/SPinPform", CompressionType.Nemesis);
			platformimg = ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/SpinningPlatforms.asm", 0, 0);
		}

		public override ReadOnlyCollection<byte> Subtypes
		{
			get { return new ReadOnlyCollection<byte>(new byte[] { 0x80, 0x81, 0x82, 0x83, 0x84, 0x85 }); }
		}

		public override string Name
		{
			get { return "Platform Conveyor Belt"; }
		}

		public override bool RememberState
		{
			get { return false; }
		}

		public override string SubtypeName(byte subtype)
		{
			return IsSpawner(subtype) ? "Platform Group " + (subtype & 0x7F) : "Platform";
		}

		public override Sprite Image
		{
			get { return platformimg; }
		}

		public override Sprite SubtypeImage(byte subtype)
		{
			return platformimg;
		}

		public override Sprite GetSprite(ObjectEntry obj)
		{
			if (!IsSpawner(obj.SubType))
				return platformimg;

			int[] group = groups[obj.SubType & 0x7F];
			List<Sprite> sprs = new List<Sprite>();
			for (int i = 0; i < group.Length; i += 2)
			{
				Sprite tmp = new Sprite(platformimg);
				tmp.Offset(group[i] - obj.X, group[i + 1] - obj.Y);
				sprs.Add(tmp);
			}
			return new Sprite(sprs.ToArray());
		}

		// Subtype bit 7 marks a group spawner; the low bits pick the group (0-5).
		private static bool IsSpawner(byte subtype)
		{
			return (subtype & 0x80) != 0 && (subtype & 0x7F) < groups.Length;
		}
	}
}
