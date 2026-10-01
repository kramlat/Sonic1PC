using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using SonicRetro.SonLVL.API;

namespace S1ObjectDefinitions.SYZ
{
	class Block : ObjectDefinition
	{
		private Sprite img;
		private List<Sprite> imgs = new List<Sprite>();

		public override void Init(ObjectData data)
		{
			byte[] artfile = ObjectHelper.LevelArt;
			img = ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/FloatingBlocksDoors.asm", 0, 2);
			for (int i = 0; i < 8; i++)
				imgs.Add(ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/FloatingBlocksDoors.asm", i, 2));
		}

		public override ReadOnlyCollection<byte> Subtypes
		{
			get { return new ReadOnlyCollection<byte>(new byte[] { 0x00, 0x01, 0x02, 0x03, 0x04, 0x10, 0x11, 0x12, 0x13, 0x14, 0x20, 0x37, 0xA0, 0x58, 0x59, 0x5A, 0x5B }); }
		}

		public override string Name
		{
			get { return "Platform"; }
		}

		public override bool RememberState
		{
			get { return false; }
		}

		public override string SubtypeName(byte subtype)
		{
			// Bit 7 makes it a switch-controlled door (low nibble = switch ID);
			// otherwise the low nibble is the movement type.
			if ((subtype & 0x80) != 0)
				return "Switch Door (Switch " + (subtype & 0x0F) + ")";
			return ((PlatformMovement)(subtype & 0x0F)).ToString();
		}

		public override Sprite Image
		{
			get { return img; }
		}

		public override Sprite SubtypeImage(byte subtype)
		{
			return imgs[(subtype & 0x70) >> 4];
		}

		public override Sprite GetSprite(ObjectEntry obj)
		{
			Sprite tmp = new Sprite(SubtypeImage(obj.SubType));
			tmp.Flip(obj.XFlip, obj.YFlip);
			return tmp;
		}

		private static readonly PropertySpec[] customProperties = new PropertySpec[] {
			new PropertySpec("Movement", typeof(PlatformMovement), "Extended", null, null, GetMovement, SetMovement),
			new PropertySpec("Switch ID", typeof(int), "Extended", null, null, GetSwitchID, SetSwitchID),
			new PropertySpec("Switch-Controlled", typeof(bool), "Extended", null, null, GetSwitchControl, SetSwitchControl)
		};

		public override PropertySpec[] CustomProperties
		{
			get { return customProperties; }
		}

		public static object GetMovement(ObjectEntry obj)
		{
			return (PlatformMovement)(obj.SubType & 0x0F);
		}

		public static void SetMovement(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~0x0F) | (int)value);
		}

		public static object GetSwitchID(ObjectEntry obj)
		{
			return (byte)(obj.SubType & 0x0F);
		}

		public static void SetSwitchID(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~0x0F) | ((byte)value & 0x0F));
		}

		public static object GetSwitchControl(ObjectEntry obj)
		{
			return (obj.SubType & 0x80) != 0 ? true : false;
		}

		public static void SetSwitchControl(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~0x80) | ((bool)value == true ? 0x80 : 0));
		}
	}

	// Movement types, matching FBlock_TypeIndex in "56 SYZ, SLZ Floating Blocks and LZ Doors.asm".
	// The high nibble of the subtype picks the block's size/frame (see FBlock_Var).
	public enum PlatformMovement
	{
		Stationary = 0,
		LeftRightSmall = 1,
		LeftRightLarge = 2,
		UpDownSmall = 3,
		UpDownLarge = 4,
		LZDoorOpen = 5,
		LZDoorClose = 6,
		HorizontalSYZ3 = 7, // SYZ3: starts moving right when switch $F is pressed
		SLZStairSmallest = 8, // moves around a square, half-range $10
		SLZStairSmall = 9, // half-range $30
		SLZStairLarge = 10, // half-range $50
		SLZStairLargest = 11, // half-range $70
		LZHorizDoorOpen = 12,
		LZHorizDoorClose = 13,
		Invalid14 = 14,
		Invalid15 = 15
	}
}
