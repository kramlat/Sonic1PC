using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using SonicRetro.SonLVL.API;

namespace S1ObjectDefinitions.MZ
{
	// Object 31 - stomping metal blocks on chains (MZ).
	//
	// One placed object builds four pieces in the game: the ceiling base, the chain,
	// the main block and (except on the small type) the spikes under it. The block
	// drops and rises along its chain by up to Length pixels.
	//
	// Subtype bits 4-6 pick the size (0 large, 1 medium, 2 small and spikeless). The
	// low nibble picks the stroke length (see CStom_Lengths). Bit 7 makes it
	// switch-activated (only one, in MZ1), which forces the plain type 0 behaviour.
	class ChainStomp : ObjectDefinition
	{
		private List<Sprite> frames = new List<Sprite>();

		// Stroke length in pixels for each value of the low nibble (high byte of CStom_Lengths).
		private static readonly int[] lengths = { 0x70, 0xA0, 0x50, 0x78, 0x38, 0x58, 0xB8 };

		public override void Init(ObjectData data)
		{
			// The mappings use tile numbers relative to the stomper's art tile ($300). The
			// spike pieces reach into the normal Spikes art at $51B (+4), i.e. tile $21B here,
			// so the two art files have to be laid out together like they are in VRAM.
			byte[] metal = ObjectHelper.OpenArtFile("../res/Art/MZMetal", CompressionType.Nemesis);
			byte[] spikes = ObjectHelper.OpenArtFile("../res/Art/Spikes", CompressionType.Nemesis);
			int spikestart = 0x21B * 32;
			byte[] art = new byte[Math.Max(metal.Length, spikestart + spikes.Length)];
			Array.Copy(metal, 0, art, 0, metal.Length);
			Array.Copy(spikes, 0, art, spikestart, spikes.Length);
			for (int i = 0; i <= 10; i++)
				frames.Add(ObjectHelper.MapASMToBmp(art, "../asm/Mappings/ChainedStompers.asm", i, 0));
		}

		public override ReadOnlyCollection<byte> Subtypes
		{
			get
			{
				return new ReadOnlyCollection<byte>(new byte[] {
					0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
					0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
					0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
					0x80 });
			}
		}

		public override string Name
		{
			get { return "Chained Stomper"; }
		}

		public override bool RememberState
		{
			get { return false; }
		}

		public override string SubtypeName(byte subtype)
		{
			if ((subtype & 0x80) != 0)
				return "Switch-Activated Stomper";
			return SizeName((subtype >> 4) & 0xF) + ", " + LengthName(subtype & 0xF);
		}

		public override Sprite Image
		{
			get { return frames[0]; }
		}

		public override Sprite SubtypeImage(byte subtype)
		{
			return frames[BlockFrame(subtype)];
		}

		public override Sprite GetSprite(ObjectEntry obj)
		{
			// A switch-activated stomper has its subtype replaced by 0 in the game.
			byte st = (obj.SubType & 0x80) != 0 ? (byte)0 : obj.SubType;
			int size = (st >> 4) & 0xF;
			int low = st & 0xF;
			// Type $x0 is created fully extended; every other length starts retracted.
			int cur = (low == 0) ? lengths[0] : 0;

			List<Sprite> sprs = new List<Sprite>();
			sprs.Add(Place(frames[2], 0, -0x10)); // base attached to the ceiling
			sprs.Add(Place(frames[3 + (cur >> 5)], 0, cur - 0x34)); // chain, one frame per $20 px
			sprs.Add(Place(frames[BlockFrame(st)], 0, cur)); // main block
			if (size != 2) // the small type has no spikes
				sprs.Add(Place(frames[1], 0, cur + 0x1C));
			return new Sprite(sprs.ToArray());
		}

		private static Sprite Place(Sprite s, int x, int y)
		{
			Sprite tmp = new Sprite(s);
			tmp.Offset(x, y);
			return tmp;
		}

		// Main block frame from subtype bits 4-6 (CStom_Var2): large, medium, small.
		private static int BlockFrame(byte subtype)
		{
			switch ((subtype >> 4) & 7)
			{
				case 1: return 9;
				case 2: return 10;
				default: return 0;
			}
		}

		private static string SizeName(int size)
		{
			switch (size)
			{
				case 0: return "Large";
				case 1: return "Medium";
				case 2: return "Small (No Spikes)";
				default: return "Invalid";
			}
		}

		private static string LengthName(int low)
		{
			return low < lengths.Length ? "Stroke " + lengths[low] + "px" + (low == 0 ? " (starts extended)" : "") : "Invalid";
		}

		private static readonly PropertySpec[] customProperties = new PropertySpec[] {
			new PropertySpec("Size", typeof(StomperSize), "Extended", "Block size. The small type has no spikes.", null, GetSize, SetSize),
			new PropertySpec("Length", typeof(int), "Extended", "Stroke length: 0 = 112px (starts extended), 1 = 160px, 2 = 80px, 3 = 120px, 4 = 56px, 5 = 88px, 6 = 184px.", null, GetLength, SetLength),
			new PropertySpec("Switch-Controlled", typeof(bool), "Extended", "Bit 7. Only used for one stomper in MZ1.", null, GetSwitch, SetSwitch)
		};

		public override PropertySpec[] CustomProperties
		{
			get { return customProperties; }
		}

		private static object GetSize(ObjectEntry obj)
		{
			return (StomperSize)((obj.SubType >> 4) & 7);
		}

		private static void SetSize(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~0x70) | ((int)(StomperSize)value << 4));
		}

		private static object GetLength(ObjectEntry obj)
		{
			return obj.SubType & 0x0F;
		}

		private static void SetLength(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~0x0F) | (Math.Min(6, Math.Max((int)value, 0))));
		}

		private static object GetSwitch(ObjectEntry obj)
		{
			return (obj.SubType & 0x80) != 0;
		}

		private static void SetSwitch(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~0x80) | ((bool)value ? 0x80 : 0));
		}
	}

	public enum StomperSize
	{
		Large,
		Medium,
		Small
	}
}
