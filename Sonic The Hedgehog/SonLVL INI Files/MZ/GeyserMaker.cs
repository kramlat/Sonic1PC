using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using SonicRetro.SonLVL.API;

namespace S1ObjectDefinitions.MZ
{
	// Object 4C - lava geyser / lavafall producer (MZ).
	//
	// The maker itself stays invisible until Sonic is close: it triggers while Sonic is
	// above it but no more than $170 px up. A geyser (subtype 0) then erupts and shoots
	// any pushable block on top of it upward; a lavafall (subtype 1) drops lava from the
	// ceiling and keeps going. The editor shows the lava bubble frame as a marker.
	class GeyserMaker : ObjectDefinition
	{
		private Sprite img;

		public override void Init(ObjectData data)
		{
			byte[] artfile = ObjectHelper.OpenArtFile("../res/Art/MZLava1", CompressionType.Nemesis);
			// ArtTile_MZ_Lava uses palette line 4.
			img = ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/LavaGeyser.asm", 0, 3);
		}

		public override ReadOnlyCollection<byte> Subtypes
		{
			get { return new ReadOnlyCollection<byte>(new byte[] { 0x00, 0x01 }); }
		}

		public override string Name
		{
			get { return "Lava Geyser Maker"; }
		}

		public override bool RememberState
		{
			get { return false; }
		}

		public override string SubtypeName(byte subtype)
		{
			return subtype == 0 ? "Geyser" : "Lavafall";
		}

		public override Sprite Image
		{
			get { return img; }
		}

		public override Sprite SubtypeImage(byte subtype)
		{
			return img;
		}

		public override Sprite GetSprite(ObjectEntry obj)
		{
			return img;
		}

		private static readonly PropertySpec[] customProperties = new PropertySpec[] {
			new PropertySpec("Type", typeof(GeyserType), "Extended", "Geyser erupts upward (and launches a pushable block); lavafall drops lava from the ceiling.", null, GetType2, SetType)
		};

		public override PropertySpec[] CustomProperties
		{
			get { return customProperties; }
		}

		private static object GetType2(ObjectEntry obj)
		{
			return (GeyserType)(obj.SubType & 1);
		}

		private static void SetType(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)(int)(GeyserType)value;
		}
	}

	public enum GeyserType
	{
		Geyser,
		Lavafall
	}
}
