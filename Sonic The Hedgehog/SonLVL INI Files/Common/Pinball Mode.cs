using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using SonicRetro.SonLVL.API;

namespace S1ObjectDefinitions.Common
{
	class PinballMode : ObjectDefinition
	{
		private Sprite img;
		private List<Sprite> imgs = new List<Sprite>();

		public override void Init(ObjectData data)
		{
			List<byte> tmpartfile = new List<byte>();
			tmpartfile.AddRange(ObjectHelper.OpenArtFile("Common/pathswapper-art.bin", CompressionType.Nemesis));
			byte[] mapfile = System.IO.File.ReadAllBytes("Common/pathswapper-map.bin");
			byte[] artfile1 = tmpartfile.ToArray();
			img = ObjectHelper.MapToBmp(artfile1, mapfile, 1, 0);
			Point off;
			BitmapBits im;
			Point pos;
			Size delta;
			for (int i = 0; i < 32; i++)
			{
				byte[] artfile = tmpartfile.GetRange(((i & 0x1C) << 5), 128).ToArray();
				BitmapBits tempim = ObjectHelper.MapToBmp(artfile, mapfile, (i & 4), 0).GetBitmap();
				if ((i & 4) != 0)
				{
					im = new BitmapBits(tempim.Width * (1 << (i & 3)), tempim.Height);
					delta = new Size(tempim.Width, 0);
				}
				else
				{
					im = new BitmapBits(tempim.Width, tempim.Height * (1 << (i & 3)));
					delta = new Size(0, tempim.Height);
				}

				pos = new Point(0, 0);
				off = new Point(-(im.Width / 2), -(im.Height / 2));
				for (int j = 0; j < (1 << (i & 3)); j++)
				{
					im.DrawBitmap(tempim, pos);
					pos = pos + delta;
				}
				imgs.Add(new Sprite(im, off));
			}
		}

		public override ReadOnlyCollection<byte> Subtypes
		{
			get { return new ReadOnlyCollection<byte>(new byte[] { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 }); }
		}

		public override string Name
		{
			get { return "Pinball Mode"; }
		}

		public override bool RememberState
		{
			get { return false; }
		}

		public override string SubtypeName(byte subtype)
		{
			string result = (subtype & 4) == 4 ? "Horizontal" : "Vertical";
			return result;
		}

		public override Sprite Image
		{
			get { return img; }
		}

		public override Sprite SubtypeImage(byte subtype)
		{
			return imgs[subtype & 0x1F];
		}

		public override Sprite GetSprite(ObjectEntry obj)
		{
			return imgs[obj.SubType & 0x1F];
		}

		public override bool Debug { get { return true; } }

		private PropertySpec[] customProperties = new PropertySpec[] {
			new PropertySpec("Size", typeof(byte), "Extended", null, null, GetSize, SetSize),
			new PropertySpec("Direction", typeof(Direction), "Extended", null, null, GetDirection, SetDirection),
			new PropertySpec("Entrance (Right/Down)", typeof(bool), "Extended", null, null, GetEntranceRD, SetEntranceRD),
			new PropertySpec("Entrance (Left/Up)", typeof(bool), "Extended", null, null, GetEntranceLU, SetEntranceLU),
			new PropertySpec("Ground only", typeof(bool), "Extended", null, null, GetGroundOnly, SetGroundOnly)
		};

		public override PropertySpec[] CustomProperties
		{
			get
			{
				return customProperties;
			}
		}

		private static object GetSize(ObjectEntry obj)
		{
			return (byte)(obj.SubType & 3);
		}

		private static void SetSize(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~3) | ((byte)value & 3));
		}

		private static object GetDirection(ObjectEntry obj)
		{
			return (obj.SubType & 4) != 0 ? Direction.Horizontal : Direction.Vertical;
		}

		private static void SetDirection(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~4) | ((Direction)value == Direction.Horizontal ? 4 : 0));
		}

		private static object GetEntranceRD(ObjectEntry obj)
		{
			return (obj.SubType & 8) != 0;
		}

		private static void SetEntranceRD(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~8) | ((bool)value ? 8 : 0));
		}

		private static object GetEntranceLU(ObjectEntry obj)
		{
			return (obj.SubType & 16) != 0;
		}

		private static void SetEntranceLU(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~16) | ((bool)value ? 16 : 0));
		}

		private static object GetGroundOnly(ObjectEntry obj)
		{
			return (obj.SubType & 128) != 0;
		}

		private static void SetGroundOnly(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)((obj.SubType & ~128) | ((bool)value ? 128 : 0));
		}
	}
}
