using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Drawing;
using SonicRetro.SonLVL.API;

namespace S1ObjectDefinitions.SBZ
{
	// Object 72 - teleporter (SBZ2). Invisible in the game: when Sonic touches it he is
	// sucked into an invisible pipe and carried along a route of fixed world
	// coordinates (Tele_Data). The subtype picks the route (0-7).
	//
	// The teleporter itself is drawn with the recomp's debug marker (a grid of icons), and
	// every point of its route with a small block icon, so the pipe can be followed in
	// the editor. Routes of one point are a straight jump; the 7-point routes (2, 4, 6)
	// follow a bend in the pipe.
	class Teleporter : ObjectDefinition
	{
		private Sprite markerimg, pointimg;

		// Route points (X, Y pairs, absolute level coordinates), from Tele_Data.
		private static readonly int[][] routes = new int[][] {
			new int[] { 0x794, 0x98C },
			new int[] { 0x94, 0x38C },
			new int[] { 0x794, 0x2E8, 0x7A4, 0x2C0, 0x7D0, 0x2AC, 0x858, 0x2AC, 0x884, 0x298, 0x894, 0x270, 0x894, 0x190 },
			new int[] { 0x894, 0x690 },
			new int[] { 0x1194, 0x470, 0x1184, 0x498, 0x1158, 0x4AC, 0xFD0, 0x4AC, 0xFA4, 0x4C0, 0xF94, 0x4E8, 0xF94, 0x590 },
			new int[] { 0x1294, 0x490 },
			new int[] { 0x1594, -0x18, 0x1584, -0x40, 0x1560, -0x54, 0x14D0, -0x54, 0x14A4, -0x68, 0x1494, -0x90, 0x1494, -0x270 },
			new int[] { 0x894, 0x90 },
		};

		public override void Init(ObjectData data)
		{
			byte[] artfile = ObjectHelper.OpenArtFile("../res/Art/Monitor", CompressionType.Nemesis);
			markerimg = ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/TeleporterMarker.asm", 0, 0);
			pointimg = ObjectHelper.MapASMToBmp(artfile, "../asm/Mappings/InvisibleBarriers.asm", 0, 0);
		}

		public override ReadOnlyCollection<byte> Subtypes
		{
			get { return new ReadOnlyCollection<byte>(new byte[] { 0, 1, 2, 3, 4, 5, 6, 7 }); }
		}

		public override string Name
		{
			get { return "Teleporter"; }
		}

		public override bool RememberState
		{
			get { return false; }
		}

		public override string SubtypeName(byte subtype)
		{
			if (subtype >= routes.Length)
				return "Invalid";
			int[] r = routes[subtype];
			int n = r.Length / 2;
			string end = "(" + Hex(r[r.Length - 2]) + ", " + Hex(r[r.Length - 1]) + ")";
			return n == 1 ? "Route " + subtype + " to " + end : "Route " + subtype + " through " + n + " points to " + end;
		}

		public override Sprite Image
		{
			get { return markerimg; }
		}

		public override Sprite SubtypeImage(byte subtype)
		{
			return markerimg;
		}

		public override Sprite GetSprite(ObjectEntry obj)
		{
			List<Sprite> sprs = new List<Sprite>();
			sprs.Add(markerimg);
			if (obj.SubType < routes.Length)
			{
				int[] r = routes[obj.SubType];
				for (int i = 0; i < r.Length; i += 2)
				{
					Sprite tmp = new Sprite(pointimg);
					tmp.Offset(r[i] - obj.X, r[i + 1] - obj.Y);
					sprs.Add(tmp);
				}
			}
			return new Sprite(sprs.ToArray());
		}

		// Signed hex for coordinates (some routes go above the top of the level).
		private static string Hex(int v)
		{
			return v < 0 ? "-$" + (-v).ToString("X") : "$" + v.ToString("X");
		}

		private static readonly PropertySpec[] customProperties = new PropertySpec[] {
			new PropertySpec("Route", typeof(int), "Extended", "Which route (0-7) the teleporter carries Sonic along.", null, GetRoute, SetRoute)
		};

		public override PropertySpec[] CustomProperties
		{
			get { return customProperties; }
		}

		private static object GetRoute(ObjectEntry obj)
		{
			return (int)obj.SubType;
		}

		private static void SetRoute(ObjectEntry obj, object value)
		{
			obj.SubType = (byte)Math.Min(routes.Length - 1, Math.Max((int)value, 0));
		}
	}
}
