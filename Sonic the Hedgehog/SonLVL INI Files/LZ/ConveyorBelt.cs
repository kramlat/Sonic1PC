using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using SonicRetro.SonLVL.API;

namespace S1ObjectDefinitions.LZ
{
	// Object 63 - platforms on a conveyor belt (LZ).
	//
	// Subtype $7F is a decorative wheel. Subtypes $80-$85 are "spawners": in the
	// game a spawner turns into a whole group of platforms spread around its belt
	// loop (8-12 of them, evenly spaced), using the placement tables in
	// objpos/platforms/ (ObjPosLZPlatform_Index: group 0/1 = LZ1, 2/3 = LZ2,
	// 4/5 = LZ3). So the editor draws the whole group, not a single platform.
	class ConveyorBelt : ObjectDefinition
	{
		private Sprite wheelimg, platformimg;

		// Start position (X, Y pairs) of every platform in each spawner group, taken
		// from objpos/platforms/lz1pf1.bin ... lz3pf2.bin. These are absolute level
		// coordinates; GetSprite draws them relative to the spawner's own position.
		private static readonly int[][] groups = new int[][] {
			// Group 0 (LZ1 platforms 1)
			new int[] {
				0x1078, 0x021A,
				0x10BE, 0x0291,
				0x10BE, 0x0307,
				0x10BE, 0x037E,
				0x105C, 0x0390,
				0x1022, 0x0352,
				0x1022, 0x02DB,
				0x1022, 0x0265,
			},
			// Group 1 (LZ1 platforms 2)
			new int[] {
				0x127E, 0x0280,
				0x12CE, 0x0305,
				0x12CE, 0x038A,
				0x12CE, 0x040F,
				0x12A7, 0x046E,
				0x1232, 0x040F,
				0x1232, 0x038A,
				0x1232, 0x0305,
			},
			// Group 2 (LZ2 platforms 1)
			new int[] {
				0x0D22, 0x0483,
				0x0D9C, 0x0482,
				0x0DAE, 0x04EA,
				0x0DAE, 0x0564,
				0x0DAE, 0x05DD,
				0x0D34, 0x05DE,
				0x0D22, 0x0576,
				0x0D22, 0x04FC,
			},
			// Group 3 (LZ2 platforms 2)
			new int[] {
				0x0D62, 0x03A2,
				0x0DD4, 0x03A2,
				0x0DEE, 0x03FA,
				0x0DEE, 0x046C,
				0x0DEE, 0x04DD,
				0x0D7C, 0x04DE,
				0x0D62, 0x0486,
				0x0D62, 0x0414,
			},
			// Group 4 (LZ3 platforms 1)
			new int[] {
				0x0CAD, 0x0242,
				0x0D2D, 0x0242,
				0x0DAC, 0x0242,
				0x0DDE, 0x028F,
				0x0DDE, 0x030E,
				0x0DDE, 0x038D,
				0x0DB0, 0x03DE,
				0x0D31, 0x03DE,
				0x0CB2, 0x03DE,
				0x0C52, 0x03BF,
				0x0C52, 0x0340,
				0x0C52, 0x02C1,
			},
			// Group 5 (LZ3 platforms 2)
			new int[] {
				0x1252, 0x020A,
				0x12D2, 0x020A,
				0x1352, 0x020A,
				0x13D2, 0x020A,
				0x13DE, 0x027E,
				0x139E, 0x02BE,
				0x131E, 0x02BE,
				0x129E, 0x02BE,
				0x1252, 0x028A,
			}		};

		public override void Init(ObjectData data)
		{
			byte[] artfile = ObjectHelper.OpenArtFile("../res/Art/LZWheel", CompressionType.Nemesis);
			wheelimg = ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/LZConveyor.asm", 0, 0);
			platformimg = ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/LZConveyor.asm", 4, 2);
		}

		public override ReadOnlyCollection<byte> Subtypes
		{
			get { return new ReadOnlyCollection<byte>(new byte[] { 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85 }); }
		}

		public override string Name
		{
			get { return "Conveyor Belt"; }
		}

		public override bool RememberState
		{
			get { return false; }
		}

		public override string SubtypeName(byte subtype)
		{
			if (subtype == 0x7F)
				return "Wheel";
			if (IsSpawner(subtype))
				return "Platform Group " + (subtype & 0x7F);
			return "Platform";
		}

		public override Sprite Image
		{
			get { return platformimg; }
		}

		public override Sprite SubtypeImage(byte subtype)
		{
			return subtype == 0x7F ? wheelimg : platformimg;
		}

		public override Sprite GetSprite(ObjectEntry obj)
		{
			if (obj.SubType == 0x7F)
				return wheelimg;

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
