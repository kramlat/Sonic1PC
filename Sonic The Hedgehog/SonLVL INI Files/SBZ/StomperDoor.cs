using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using SonicRetro.SonLVL.API;

namespace S1ObjectDefinitions.SBZ
{
	// Object 6B - stomper and sliding door (SBZ1/SBZ2), and the "ancient lift"
	// at the start of SBZ3 (LZ act 4).
	//
	// The high nibble of the subtype (bits 4-6) picks the size/frame (see Sto_Var):
	//   0 sliding door that extends when its switch is pressed
	//   1 stomper that drops and rises slowly    2 stomper, short travel
	//   3 stomper, long travel                   4 ancient lift (SBZ3)
	// With bit 7 set the low nibble is the switch ID that triggers it; otherwise the
	// low nibble is the movement type directly.
	class StomperDoor : ObjectDefinition
	{
		private List<Sprite> imgs = new List<Sprite>();

		public override void Init(ObjectData data)
		{
			byte[] artfile = ObjectHelper.OpenArtFile("../res/Art/Stomper", CompressionType.Nemesis);
			for (int i = 0; i < 4; i++)
				imgs.Add(ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/StomperDoor.asm", i, 1));

			// The ancient lift's art is part of the level graphics, starting at tile $1F0
			// (ArtTile_Level+$1F0), on palette line 3.
			byte[] levelart = ObjectHelper.LevelArt;
			int start = 0x1F0 * 32;
			byte[] liftart = new byte[levelart.Length > start ? levelart.Length - start : 0];
			if (liftart.Length > 0)
				Array.Copy(levelart, start, liftart, 0, liftart.Length);
			imgs.Add(ObjectHelper.MapASMToBmp(liftart, "../asm/Mappings/StomperDoor.asm", 4, 2));
		}

		public override ReadOnlyCollection<byte> Subtypes
		{
			get { return new ReadOnlyCollection<byte>(new byte[] { 0x80, 0x13, 0x24, 0x34, 0x40, 0xC0 }); }
		}

		public override string Name
		{
			get { return "Stomper and Door"; }
		}

		public override bool RememberState
		{
			get { return false; }
		}

		public override string SubtypeName(byte subtype)
		{
			string kind;
			switch ((subtype >> 4) & 7)
			{
				case 0: kind = "Sliding Door"; break;
				case 1: kind = "Stomper (Slow Rise)"; break;
				case 2: kind = "Stomper (Short)"; break;
				case 3: kind = "Stomper (Long)"; break;
				case 4: kind = "Ancient Lift"; break;
				default: kind = "Invalid"; break;
			}
			if ((subtype & 0x80) != 0)
				kind += " (Switch " + (subtype & 0x0F) + ")";
			return kind;
		}

		public override Sprite Image
		{
			get { return imgs[1]; }
		}

		public override Sprite SubtypeImage(byte subtype)
		{
			return FrameFor(subtype);
		}

		public override Sprite GetSprite(ObjectEntry obj)
		{
			return FrameFor(obj.SubType);
		}

		// Frame = subtype bits 4-6; anything past the lift falls back to the door.
		private Sprite FrameFor(byte subtype)
		{
			int frame = (subtype >> 4) & 7;
			return imgs[frame < imgs.Count ? frame : 0];
		}
	}
}
