#include "chisnes.h"
#include "apu.h"
#include "cpuexec.h"
#include "display.h"
#include "dma.h"
#include "fxemu.h"
#include "fxinst.h"
#include "memmap.h"
#include "ppu.h"
#include "sa1.h"
#include "sdd1.h"
#include "srtc.h"
#include "spc7110.h"
#include "gfx.h"

extern uint8_t mul_brightness[16][32];

uint32_t justifiers = 0xffff00aa;
uint8_t in_bit = 0;

extern uint8_t* HDMAMemPointers[8];

void LatchCounters(bool force)
{
	if (!force && !(Memory.FillRAM[0x4213] & 0x80))
		return;

	PPU.VBeamPosLatched = (uint16_t) CPU.V_Counter;
	PPU.HBeamPosLatched = (uint16_t) ((CPU.Cycles * SNES_MAX_HCOUNTER) / Settings.H_Max);
	Memory.FillRAM[0x213f] |= 0x40;
}

void UpdateJustifiers();

void UpdateHTimer()
{
	if (!PPU.HTimerEnabled)
		return;

	PPU.HTimerPosition = PPU.IRQHBeamPos * Settings.H_Max / SNES_MAX_HCOUNTER;

	if (PPU.HTimerPosition == Settings.H_Max || PPU.HTimerPosition == Settings.HBlankStart)
		PPU.HTimerPosition--;

	if (!PPU.VTimerEnabled || CPU.V_Counter == PPU.IRQVBeamPos)
	{
		if (PPU.HTimerPosition < CPU.Cycles)
		{
			/* Missed the IRQ on this line already */
			if (CPU.WhichEvent == HBLANK_END_EVENT || CPU.WhichEvent == HTIMER_AFTER_EVENT)
			{
				CPU.WhichEvent = HBLANK_END_EVENT;
				CPU.NextEvent = Settings.H_Max;
			}
			else
			{
				CPU.WhichEvent = HBLANK_START_EVENT;
				CPU.NextEvent = Settings.HBlankStart;
			}
		}
		else
		{
			if (CPU.WhichEvent == HTIMER_BEFORE_EVENT || CPU.WhichEvent == HBLANK_START_EVENT)
			{
				if (PPU.HTimerPosition > Settings.HBlankStart)
				{
					/* HTimer was to trigger before h-blank start, now triggers after start of h-blank */
					CPU.NextEvent = Settings.HBlankStart;
					CPU.WhichEvent = HBLANK_START_EVENT;
				}
				else
				{
					CPU.NextEvent = PPU.HTimerPosition;
					CPU.WhichEvent = HTIMER_BEFORE_EVENT;
				}
			}
			else
			{
				CPU.WhichEvent = HTIMER_AFTER_EVENT;
				CPU.NextEvent = PPU.HTimerPosition;
			}
		}
	}
}

void FixColourBrightness()
{
	int32_t i;
	IPPU.XB = mul_brightness[PPU.Brightness];

	for (i = 0; i < 256; i++)
	{
		IPPU.Red[i] = IPPU.XB[PPU.CGDATA[i] & 0x1f];
		IPPU.Green[i] = IPPU.XB[(PPU.CGDATA[i] >> 5) & 0x1f];
		IPPU.Blue[i] = IPPU.XB[(PPU.CGDATA[i] >> 10) & 0x1f];
		IPPU.ScreenColors[i] = BUILD_PIXEL(IPPU.Red[i], IPPU.Green[i], IPPU.Blue[i]);
	}
}

void SetPPU(uint8_t Byte, uint16_t Address)
{
	if (Address >= 0x2188)
	{
		if ((Settings.Chip & GSU) == GSU && Address >= 0x3000 && Address <= 0x32ff)
		{
			SetSuperFX(Byte, Address);
			return;
		}
		else if (Settings.Chip == SA_1 && Address >= 0x2200 && Address <= 0x23ff)
		{
			SetSA1(Byte, Address);
			return;
		}
		else if (Settings.Chip == S_RTC && Address == 0x2801)
			SetSRTC(Byte, Address);

		ICPU.OpenBus = Memory.FillRAM[Address] = Byte;
		return;
	}

	switch (Address)
	{
		case 0x2100: /* Brightness and screen blank bit */
			if (Byte == Memory.FillRAM[0x2100])
				break;

			FLUSH_REDRAW();

			if (PPU.Brightness != (Byte & 0xf))
			{
				IPPU.ColorsChanged               = true;
				IPPU.DirectColourMapsNeedRebuild = true;
				PPU.Brightness                   = Byte & 0xf;
				FixColourBrightness();
			}
			if ((Memory.FillRAM[0x2100] & 0x80) != (Byte & 0x80))
			{
				IPPU.ColorsChanged = true;
				PPU.ForcedBlanking = (Byte >> 7) & 1;
			}

			break;
		case 0x2101: /* Sprite (OBJ) tile address */
			if (Byte == Memory.FillRAM[0x2101])
				break;

			FLUSH_REDRAW();
			PPU.OBJNameBase   = (Byte & 3) << 14;
			PPU.OBJNameSelect = ((Byte >> 3) & 3) << 13;
			PPU.OBJSizeSelect = (Byte >> 5) & 7;
			IPPU.OBJChanged   = true;
			break;
		case 0x2102: /* Sprite write address (low) */
			PPU.OAMAddr      = ((Memory.FillRAM[0x2103] & 1) << 8) | Byte;
			PPU.OAMFlip      = 2;
			PPU.SavedOAMAddr = PPU.OAMAddr;

			if (PPU.OAMPriorityRotation && PPU.FirstSprite != (PPU.OAMAddr >> 1))
			{
				PPU.FirstSprite = (PPU.OAMAddr & 0xFE) >> 1;
				IPPU.OBJChanged = true;
			}

			break;
		case 0x2103: /* Sprite register write address (high), sprite priority rotation bit. */
			PPU.OAMAddr = ((Byte & 1) << 8) | Memory.FillRAM[0x2102];
			PPU.OAMPriorityRotation = (Byte & 0x80) ? 1 : 0;

			if (PPU.OAMPriorityRotation)
			{
				if (PPU.FirstSprite != (PPU.OAMAddr >> 1))
				{
					PPU.FirstSprite = (PPU.OAMAddr & 0xfe) >> 1;
					IPPU.OBJChanged = true;
				}
			}
			else
			{
				if (PPU.FirstSprite != 0)
				{
					PPU.FirstSprite = 0;
					IPPU.OBJChanged = true;
				}
			}

			PPU.OAMFlip      = 0;
			PPU.SavedOAMAddr = PPU.OAMAddr;
			break;
		case 0x2104: /* Sprite register write */
			REGISTER_2104(Byte);
			break;
		case 0x2105: /* Screen mode (0 - 7), background tile sizes and background 3 priority */
			if (Byte == Memory.FillRAM[0x2105])
				break;

			FLUSH_REDRAW();
			PPU.BG[0].BGSize = (Byte >> 4) & 1;
			PPU.BG[1].BGSize = (Byte >> 5) & 1;
			PPU.BG[2].BGSize = (Byte >> 6) & 1;
			PPU.BG[3].BGSize = (Byte >> 7) & 1;
			PPU.BGMode       = Byte & 7;
			/* BJ: BG3Priority only takes effect if BGMode == 1 and the bit is set */
			PPU.BG3Priority = ((Byte & 0x0f) == 0x09);

			if (PPU.BGMode == 5 || PPU.BGMode == 6 || PPU.BGMode == 7)
				IPPU.Interlace = (bool) (Memory.FillRAM[0x2133] & 1);
			else
				IPPU.Interlace = 0;

			break;
		case 0x2106: /* Mosaic pixel size and enable */
			if (Byte == Memory.FillRAM[0x2106])
				break;

			FLUSH_REDRAW();
			PPU.Mosaic      = (Byte >> 4) + 1;
			PPU.BGMosaic[0] = (Byte & 1);
			PPU.BGMosaic[1] = (Byte & 2);
			PPU.BGMosaic[2] = (Byte & 4);
			PPU.BGMosaic[3] = (Byte & 8);
			break;
		case 0x2107: /* [BG0SC] */
		case 0x2108: /* [BG1SC] */
		case 0x2109: /* [BG2SC] */
		case 0x210a: /* [BG3SC] */
			if (Byte == Memory.FillRAM[Address])
				break;

			FLUSH_REDRAW();
			PPU.BG[Address - 0x2107].SCSize = Byte & 3;
			PPU.BG[Address - 0x2107].SCBase = (Byte & 0x7c) << 8;
			break;
		case 0x210b: /* [BG01NBA] */
			if (Byte == Memory.FillRAM[0x210b])
				break;

			FLUSH_REDRAW();
			PPU.BG[0].NameBase = (Byte & 7) << 12;
			PPU.BG[1].NameBase = ((Byte >> 4) & 7) << 12;
			break;
		case 0x210c: /* [BG23NBA] */
			if (Byte == Memory.FillRAM[0x210c])
				break;

			FLUSH_REDRAW();
			PPU.BG[2].NameBase = (Byte & 7) << 12;
			PPU.BG[3].NameBase = ((Byte >> 4) & 7) << 12;
			break;
		case 0x210d: // BG1HOFS, M7HOFS
			PPU.BG[0].HOffset = (Byte << 8) | (PPU.BGnxOFSbyte & ~7) | ((PPU.BG[0].HOffset >> 8) & 7);
			PPU.BGnxOFSbyte   = Byte;
			break;
		case 0x210e: // BG1VOFS, M7VOFS
			PPU.BG[0].VOffset = (Byte << 8) | PPU.BGnxOFSbyte;
			PPU.BGnxOFSbyte   = Byte;
			break;
		case 0x210f: // BG2HOFS
			PPU.BG[1].HOffset = (Byte << 8) | (PPU.BGnxOFSbyte & ~7) | ((PPU.BG[1].HOffset >> 8) & 7);
			PPU.BGnxOFSbyte   = Byte;
			break;
		case 0x2110: // BG2VOFS
			PPU.BG[1].VOffset = (Byte << 8) | PPU.BGnxOFSbyte;
			PPU.BGnxOFSbyte   = Byte;
			break;
		case 0x2111: // BG3HOFS
			PPU.BG[2].HOffset = (Byte << 8) | (PPU.BGnxOFSbyte & ~7) | ((PPU.BG[2].HOffset >> 8) & 7);
			PPU.BGnxOFSbyte   = Byte;
			break;
		case 0x2112: // BG3VOFS
			PPU.BG[2].VOffset = (Byte << 8) | PPU.BGnxOFSbyte;
			PPU.BGnxOFSbyte   = Byte;
			break;
		case 0x2113: // BG4HOFS
			PPU.BG[3].HOffset = (Byte << 8) | (PPU.BGnxOFSbyte & ~7) | ((PPU.BG[3].HOffset >> 8) & 7);
			PPU.BGnxOFSbyte   = Byte;
			break;
		case 0x2114: // BG4VOFS
			PPU.BG[3].VOffset = (Byte << 8) | PPU.BGnxOFSbyte;
			PPU.BGnxOFSbyte   = Byte;
			break;
		case 0x2115: /* VRAM byte/word access flag and increment */
			PPU.VMA.High = (bool) (Byte & 0x80);

			switch (Byte & 3)
			{
				case 0:
					PPU.VMA.Increment = 1;
					break;
				case 1:
					PPU.VMA.Increment = 32;
					break;
				case 2:
				case 3:
					PPU.VMA.Increment = 128;
					break;
			}

			if (Byte & 0x0c)
			{
				static uint16_t Shift[4]    = {0, 5, 6, 7};
				static uint16_t IncCount[4] = {0, 32, 64, 128};
				uint8_t         i           = (Byte & 0x0c) >> 2;
				PPU.VMA.FullGraphicCount    = IncCount[i];
				PPU.VMA.Mask1               = IncCount[i] * 8 - 1;
				PPU.VMA.Shift               = Shift[i];
			}
			else
				PPU.VMA.FullGraphicCount = 0;

			break;
		case 0x2116: /* VRAM read/write address (low) */
			PPU.VMA.Address &= 0xff00;
			PPU.VMA.Address |= Byte;
			IPPU.FirstVRAMRead = true;
			break;
		case 0x2117: /* VRAM read/write address (high) */
			PPU.VMA.Address &= 0x00ff;
			PPU.VMA.Address |= Byte << 8;
			IPPU.FirstVRAMRead = true;
			break;
		case 0x2118: /* VRAM write data (low) */
			IPPU.FirstVRAMRead = true;
			REGISTER_2118(Byte);
			break;
		case 0x2119: /* VRAM write data (high) */
			IPPU.FirstVRAMRead = true;
			REGISTER_2119(Byte);
			break;
		case 0x211a: /* Mode 7 outside rotation area display mode and flipping */
			if (Byte == Memory.FillRAM[0x211a])
				break;

			FLUSH_REDRAW();
			PPU.Mode7Repeat = Byte >> 6;

			if (PPU.Mode7Repeat == 1)
				PPU.Mode7Repeat = 0;

			PPU.Mode7VFlip = (bool) (Byte & 2);
			PPU.Mode7HFlip = (bool) (Byte & 1);
			break;
		case 0x211b: /* Mode 7 matrix A (low & high) */
			PPU.MatrixA          = ((PPU.MatrixA >> 8) & 0xff) | (Byte << 8);
			PPU.Need16x8Multiply = true;
			break;
		case 0x211c: /* Mode 7 matrix B (low & high) */
			PPU.MatrixB          = ((PPU.MatrixB >> 8) & 0xff) | (Byte << 8);
			PPU.Need16x8Multiply = true;
			break;
		case 0x211d: /* Mode 7 matrix C (low & high) */
			PPU.MatrixC = ((PPU.MatrixC >> 8) & 0xff) | (Byte << 8);
			break;
		case 0x211e: /* Mode 7 matrix D (low & high) */
			PPU.MatrixD = ((PPU.MatrixD >> 8) & 0xff) | (Byte << 8);
			break;
		case 0x211f: /* Mode 7 centre of rotation X (low & high) */
			PPU.CentreX = ((PPU.CentreX >> 8) & 0xff) | (Byte << 8);
			break;
		case 0x2120: /* Mode 7 centre of rotation Y (low & high) */
			PPU.CentreY = ((PPU.CentreY >> 8) & 0xff) | (Byte << 8);
			break;
		case 0x2121: /* CG-RAM address */
			PPU.CGFLIP     = false;
			PPU.CGFLIPRead = false;
			PPU.CGADD      = Byte;
			break;
		case 0x2122: /* CGDATA */
			REGISTER_2122(Byte);
			break;
		case 0x2123: /* Window 1 and 2 enable for backgrounds 1 and 2 */
			if (Byte == Memory.FillRAM[0x2123])
				break;

			FLUSH_REDRAW();
			PPU.ClipWindow1Enable[0] = !!(Byte & 0x02);
			PPU.ClipWindow1Enable[1] = !!(Byte & 0x20);
			PPU.ClipWindow2Enable[0] = !!(Byte & 0x08);
			PPU.ClipWindow2Enable[1] = !!(Byte & 0x80);
			PPU.ClipWindow1Inside[0] = !(Byte & 0x01);
			PPU.ClipWindow1Inside[1] = !(Byte & 0x10);
			PPU.ClipWindow2Inside[0] = !(Byte & 0x04);
			PPU.ClipWindow2Inside[1] = !(Byte & 0x40);
			PPU.RecomputeClipWindows = true;
			break;
		case 0x2124: /* Window 1 and 2 enable for backgrounds 3 and 4 */
			if (Byte == Memory.FillRAM[0x2124])
				break;

			FLUSH_REDRAW();
			PPU.ClipWindow1Enable[2] = !!(Byte & 0x02);
			PPU.ClipWindow1Enable[3] = !!(Byte & 0x20);
			PPU.ClipWindow2Enable[2] = !!(Byte & 0x08);
			PPU.ClipWindow2Enable[3] = !!(Byte & 0x80);
			PPU.ClipWindow1Inside[2] = !(Byte & 0x01);
			PPU.ClipWindow1Inside[3] = !(Byte & 0x10);
			PPU.ClipWindow2Inside[2] = !(Byte & 0x04);
			PPU.ClipWindow2Inside[3] = !(Byte & 0x40);
			PPU.RecomputeClipWindows = true;
			break;
		case 0x2125: /* Window 1 and 2 enable for objects and colour window */
			if (Byte == Memory.FillRAM[0x2125])
				break;

			FLUSH_REDRAW();
			PPU.ClipWindow1Enable[4] = !!(Byte & 0x02);
			PPU.ClipWindow1Enable[5] = !!(Byte & 0x20);
			PPU.ClipWindow2Enable[4] = !!(Byte & 0x08);
			PPU.ClipWindow2Enable[5] = !!(Byte & 0x80);
			PPU.ClipWindow1Inside[4] = !(Byte & 0x01);
			PPU.ClipWindow1Inside[5] = !(Byte & 0x10);
			PPU.ClipWindow2Inside[4] = !(Byte & 0x04);
			PPU.ClipWindow2Inside[5] = !(Byte & 0x40);
			PPU.RecomputeClipWindows = true;
			break;
		case 0x2126: /* Window 1 left position */
			if (Byte == Memory.FillRAM[0x2126])
				break;

			FLUSH_REDRAW();
			PPU.Window1Left = Byte;
			PPU.RecomputeClipWindows = true;
			break;
		case 0x2127: /* Window 1 right position */
			if (Byte == Memory.FillRAM[0x2127])
				break;

			FLUSH_REDRAW();
			PPU.Window1Right         = Byte;
			PPU.RecomputeClipWindows = true;
			break;
		case 0x2128: /* Window 2 left position */
			if (Byte == Memory.FillRAM[0x2128])
				break;

			FLUSH_REDRAW();
			PPU.Window2Left = Byte;
			PPU.RecomputeClipWindows = true;
			break;
		case 0x2129: /* Window 2 right position */
			if (Byte == Memory.FillRAM[0x2129])
				break;

			FLUSH_REDRAW();
			PPU.Window2Right = Byte;
			PPU.RecomputeClipWindows = true;
			break;
		case 0x212a: /* Windows 1 & 2 overlap logic for backgrounds 1 - 4 */
			if (Byte == Memory.FillRAM[0x212a])
				break;

			FLUSH_REDRAW();
			PPU.ClipWindowOverlapLogic[0] = (Byte & 0x03);
			PPU.ClipWindowOverlapLogic[1] = (Byte & 0x0c) >> 2;
			PPU.ClipWindowOverlapLogic[2] = (Byte & 0x30) >> 4;
			PPU.ClipWindowOverlapLogic[3] = (Byte & 0xc0) >> 6;
			PPU.RecomputeClipWindows = true;
			break;
		case 0x212b: /* Windows 1 & 2 overlap logic for objects and colour window */
			if (Byte == Memory.FillRAM[0x212b])
				break;

			FLUSH_REDRAW();
			PPU.ClipWindowOverlapLogic[4] = Byte & 0x03;
			PPU.ClipWindowOverlapLogic[5] = (Byte & 0x0c) >> 2;
			PPU.RecomputeClipWindows      = true;
			break;
		case 0x212c: /* Main screen designation (backgrounds 1 - 4 and objects) */
			if (Byte == Memory.FillRAM[0x212c])
				break;

			FLUSH_REDRAW();
			PPU.RecomputeClipWindows = true;
			break;
		case 0x212d: /* Sub-screen designation (backgrounds 1 - 4 and objects) */
		case 0x212e: /* Window mask designation for main screen ? */
		case 0x212f: /* Window mask designation for sub-screen ? */
		case 0x2130: /* Fixed colour addition or screen addition */
			if (Byte == Memory.FillRAM[Address])
				break;

			FLUSH_REDRAW();
			PPU.RecomputeClipWindows = true;
			break;
		case 0x2131: /* Colour addition or subtraction select */
			if (Byte == Memory.FillRAM[0x2131])
				break;

			FLUSH_REDRAW();
			break;
		case 0x2132: /* COLDATA */
			if (Byte == Memory.FillRAM[0x2132])
				break;

			FLUSH_REDRAW();

			if (Byte & 0x80)
				PPU.FixedColourBlue = Byte & 0x1f;

			if (Byte & 0x40)
				PPU.FixedColourGreen = Byte & 0x1f;

			if (Byte & 0x20)
				PPU.FixedColourRed = Byte & 0x1f;

			break;
		case 0x2133: /* Screen settings */
			if (Byte == Memory.FillRAM[0x2133])
				break;

			if ((Memory.FillRAM[0x2133] ^ Byte) & 8)
			{
				FLUSH_REDRAW();
				GFX.Pseudo = Byte & 8;
			}

			if (Byte & 0x04)
				PPU.ScreenHeight = SNES_HEIGHT_EXTENDED;
			else
				PPU.ScreenHeight = SNES_HEIGHT;

			if (IPPU.DoubleHeightPixels)
				IPPU.RenderedScreenHeight = PPU.ScreenHeight << 1;
			else
				IPPU.RenderedScreenHeight = PPU.ScreenHeight;

			if (!((Memory.FillRAM[0x2133] ^ Byte) & 3))
				break;

			FLUSH_REDRAW();

			if ((Memory.FillRAM[0x2133] ^ Byte) & 2)
				IPPU.OBJChanged = true;

			if (PPU.BGMode == 5 || PPU.BGMode == 6)
				IPPU.Interlace = Byte & 1;

			break;
		case 0x2134: /* MPYL */
		case 0x2135: /* MPYM */
		case 0x2136: /* Matrix 16bit x 8bit multiply result (read-only) */
		case 0x2137: /* Software latch for horizontal and vertical timers (read-only) */
		case 0x2138: /* OAM read data (read-only) */
		case 0x2139: /* VMDATALREAD */
		case 0x213a: /* VRAM read data (read-only) */
		case 0x213b: /* CG-RAM read data (read-only) */
		case 0x213c: /* OPHCT */
		case 0x213d: /* Horizontal and vertical (low/high) read counter (read-only) */
		case 0x213e: /* PPU status (time over and range over) */
		case 0x213f: /* NTSC/PAL select and field (read-only) */
			return;
		case 0x2140: /* APUIO0, APUIO1, APUIO2, APUIO3 */
		case 0x2141:
		case 0x2142:
		case 0x2143:
		case 0x2144:
		case 0x2145:
		case 0x2146:
		case 0x2147:
		case 0x2148:
		case 0x2149:
		case 0x214a:
		case 0x214b:
		case 0x214c:
		case 0x214d:
		case 0x214e:
		case 0x214f:
		case 0x2150:
		case 0x2151:
		case 0x2152:
		case 0x2153:
		case 0x2154:
		case 0x2155:
		case 0x2156:
		case 0x2157:
		case 0x2158:
		case 0x2159:
		case 0x215a:
		case 0x215b:
		case 0x215c:
		case 0x215d:
		case 0x215e:
		case 0x215f:
		case 0x2160:
		case 0x2161:
		case 0x2162:
		case 0x2163:
		case 0x2164:
		case 0x2165:
		case 0x2166:
		case 0x2167:
		case 0x2168:
		case 0x2169:
		case 0x216a:
		case 0x216b:
		case 0x216c:
		case 0x216d:
		case 0x216e:
		case 0x216f:
		case 0x2170:
		case 0x2171:
		case 0x2172:
		case 0x2173:
		case 0x2174:
		case 0x2175:
		case 0x2176:
		case 0x2177:
		case 0x2178:
		case 0x2179:
		case 0x217a:
		case 0x217b:
		case 0x217c:
		case 0x217d:
		case 0x217e:
		case 0x217f:
			if (Settings.APUEnabled)
				APUMainLoop();

			IAPU.RAM[(Address & 3) | 0xf4] = Byte;
			IAPU.Executing = Settings.APUEnabled;
			IAPU.WaitCounter = 1;
			break;
		case 0x2180: // WMDATA
			if (!CPU.InDMA)
				REGISTER_2180(Byte);

			break;
		case 0x2181: // WMADDL
			if (CPU.InDMA)
				break;

			PPU.WRAM &= 0x1ff00;
			PPU.WRAM |= Byte;
			break;
		case 0x2182: // WMADDM
			if (CPU.InDMA)
				break;

			PPU.WRAM &= 0x100ff;
			PPU.WRAM |= Byte << 8;
			break;
		case 0x2183: // WMADDH
			if (CPU.InDMA)
				break;

			PPU.WRAM &= 0x0ffff;
			PPU.WRAM |= Byte << 16;
			PPU.WRAM &= 0x1ffff;
			break;
	}

	ICPU.OpenBus = Memory.FillRAM[Address] = Byte;
}

uint8_t GetPPU(uint16_t Address)
{
	uint8_t byte;

	if (Address < 0x2100) /* not a real PPU reg */
		return PPU.OpenBus1;  /* treat as unmapped memory returning last byte on the bus */

	if (Address >= 0x2188)
	{
		if (((Settings.Chip & GSU) == GSU) && Address >= 0x3000 && Address <= 0x32ff)
			return GetSuperFX(Address);
		else if (Settings.Chip == SA_1 && Address >= 0x2200)
			return GetSA1(Address);
		else if (Settings.Chip == S_RTC && Address == 0x2800)
			return GetSRTC(Address);
		else if (Address == 0x21c2)
			return (Model->_5C77 == 2) ? 0x20 : PPU.OpenBus2;
		else if (Address == 0x21c3)
			return (Model->_5C77 == 2) ? 0 : PPU.OpenBus2;

		return ICPU.OpenBus;
	}

	switch (Address)
	{
		case 0x2104: /* OAMDATA */
		case 0x2105: /* BGMODE */
		case 0x2106: /* MOSAIC */
		case 0x2108: /* BG2SC */
		case 0x2109: /* BG3SC */
		case 0x210a: /* BG4SC */
		case 0x2114: /* BG4VOFS */
		case 0x2115: /* VMAIN */
		case 0x2116: /* VMADDL */
		case 0x2118: /* VMDATAL */
		case 0x2119: /* VMDATAH */
		case 0x211a: /* M7SEL */
		case 0x2124: /* W34SEL */
		case 0x2125: /* WOBJSEL */
		case 0x2126: /* WH0 */
		case 0x2128: /* WH2 */
		case 0x2129: /* WH3 */
		case 0x212a: /* WBGLOG */
			return PPU.OpenBus1;
		case 0x2134: /* MPYL */
		case 0x2135: /* MPYM */
		case 0x2136: /* MPYH */
			if (PPU.Need16x8Multiply)
			{
				int32_t r              = (int32_t) PPU.MatrixA * (int32_t) (PPU.MatrixB >> 8);
				Memory.FillRAM[0x2134] = (uint8_t) r;
				Memory.FillRAM[0x2135] = (uint8_t) (r >> 8);
				Memory.FillRAM[0x2136] = (uint8_t) (r >> 16);
				PPU.Need16x8Multiply   = false;
			}

			PPU.OpenBus1 = Memory.FillRAM[Address];
			return PPU.OpenBus1;
		case 0x2137: /* SLHV */
			LatchCounters(0);
			return ICPU.OpenBus;
		case 0x2138: /* Read OAM (sprite) control data */
			if (PPU.OAMAddr & 0x100)
			{
				if (PPU.OAMFlip & 1)
				{
					byte = PPU.OAMData[((PPU.OAMAddr & 0x10f) << 1) + 1];
					PPU.OAMAddr = (PPU.OAMAddr + 1) & 0x1ff;

					if (PPU.OAMPriorityRotation && PPU.FirstSprite != (PPU.OAMAddr >> 1))
					{
						PPU.FirstSprite = (PPU.OAMAddr & 0xfe) >> 1;
						IPPU.OBJChanged = true;
					}
				}
				else
					byte = PPU.OAMData[(PPU.OAMAddr & 0x10f) << 1];
			}
			else if (PPU.OAMFlip & 1)
			{
				byte = PPU.OAMData[(PPU.OAMAddr << 1) + 1];
				++PPU.OAMAddr;

				if (PPU.OAMPriorityRotation && PPU.FirstSprite != (PPU.OAMAddr >> 1))
				{
					PPU.FirstSprite = (PPU.OAMAddr & 0xfe) >> 1;
					IPPU.OBJChanged = true;
				}
			}
			else
				byte = PPU.OAMData[PPU.OAMAddr << 1];

			PPU.OAMFlip ^= 1;
			PPU.OpenBus1 = byte;
			return PPU.OpenBus1;
		case 0x2139: /* Read VRAM low byte */
			if (IPPU.FirstVRAMRead)
				byte = Memory.VRAM[(PPU.VMA.Address << 1) & 0xffff];
			else if (PPU.VMA.FullGraphicCount)
			{
				uint32_t addr = PPU.VMA.Address - 1;
				uint32_t rem = addr & PPU.VMA.Mask1;
				uint32_t address = (addr & ~PPU.VMA.Mask1) + (rem >> PPU.VMA.Shift) + ((rem & (PPU.VMA.FullGraphicCount - 1)) << 3);
				byte = Memory.VRAM[((address << 1) - 2) & 0xffff];
			}
			else
				byte = Memory.VRAM[((PPU.VMA.Address << 1) - 2) & 0xffff];

			if (!PPU.VMA.High)
			{
				PPU.VMA.Address += PPU.VMA.Increment;
				IPPU.FirstVRAMRead = false;
			}

			PPU.OpenBus1 = byte;
			return PPU.OpenBus1;
		case 0x213a: /* Read VRAM high byte */
			if (IPPU.FirstVRAMRead)
				byte = Memory.VRAM[((PPU.VMA.Address << 1) + 1) & 0xffff];
			else if (PPU.VMA.FullGraphicCount)
			{
				uint32_t addr = PPU.VMA.Address - 1;
				uint32_t rem     = addr & PPU.VMA.Mask1;
				uint32_t address = (addr & ~PPU.VMA.Mask1) + (rem >> PPU.VMA.Shift) + ((rem & (PPU.VMA.FullGraphicCount - 1)) << 3);
				byte = Memory.VRAM[((address << 1) - 1) & 0xffff];
			}
			else
				byte = Memory.VRAM[((PPU.VMA.Address << 1) - 1) & 0xffff];

			if (PPU.VMA.High)
			{
				PPU.VMA.Address += PPU.VMA.Increment;
				IPPU.FirstVRAMRead = false;
			}

			PPU.OpenBus1 = byte;
			return PPU.OpenBus1;
		case 0x213b: /* Read palette data */
			if (PPU.CGFLIPRead)
				byte = (PPU.OpenBus2 & 0x80) | ((PPU.CGDATA[PPU.CGADD++] >> 8) & 0x7f);
			else
				byte = PPU.CGDATA[PPU.CGADD] & 0xff;

			PPU.CGFLIPRead = !PPU.CGFLIPRead;
			PPU.OpenBus2 = byte;
			return PPU.OpenBus2;
		case 0x213c: /* Horizontal counter value 0-339 */
			if (PPU.HBeamFlip)
				byte = (PPU.OpenBus2 & 0xfe) | ((PPU.HBeamPosLatched >> 8) & 0x01);
			else
				byte = (uint8_t) PPU.HBeamPosLatched;

			PPU.HBeamFlip ^= 1;
			PPU.OpenBus2 = byte;
			return PPU.OpenBus2;
		case 0x213d: /* Vertical counter value 0-262 */
			if (PPU.VBeamFlip)
				byte = (PPU.OpenBus2 & 0xfe) | ((PPU.VBeamPosLatched >> 8) & 0x01);
			else
				byte = (uint8_t) PPU.VBeamPosLatched;

			PPU.VBeamFlip ^= 1;
			PPU.OpenBus2 = byte;
			return PPU.OpenBus2;
		case 0x213e: /* PPU time and range over flags */
			FLUSH_REDRAW();
			byte = (PPU.OpenBus1 & 0x10) | PPU.RangeTimeOver | Model->_5C77;
			PPU.OpenBus1 = byte;
			return PPU.OpenBus1;
		case 0x213f: /* NTSC/PAL and which field flags */
			PPU.VBeamFlip = PPU.HBeamFlip = 0;
			byte = (PPU.OpenBus2 & 0x20) | (Memory.FillRAM[0x213f] & 0xc0) | (Settings.PAL ? 0x10 : 0) | Model->_5C78;
			Memory.FillRAM[0x213f] &= ~0x40;
			PPU.OpenBus2 = byte;
			return PPU.OpenBus2;
		case 0x2140: /* APUIO0, APUIO1, APUIO2, APUIO3 */
		case 0x2141:
		case 0x2142:
		case 0x2143:
		case 0x2144:
		case 0x2145:
		case 0x2146:
		case 0x2147:
		case 0x2148:
		case 0x2149:
		case 0x214a:
		case 0x214b:
		case 0x214c:
		case 0x214d:
		case 0x214e:
		case 0x214f:
		case 0x2150:
		case 0x2151:
		case 0x2152:
		case 0x2153:
		case 0x2154:
		case 0x2155:
		case 0x2156:
		case 0x2157:
		case 0x2158:
		case 0x2159:
		case 0x215a:
		case 0x215b:
		case 0x215c:
		case 0x215d:
		case 0x215e:
		case 0x215f:
		case 0x2160:
		case 0x2161:
		case 0x2162:
		case 0x2163:
		case 0x2164:
		case 0x2165:
		case 0x2166:
		case 0x2167:
		case 0x2168:
		case 0x2169:
		case 0x216a:
		case 0x216b:
		case 0x216c:
		case 0x216d:
		case 0x216e:
		case 0x216f:
		case 0x2170:
		case 0x2171:
		case 0x2172:
		case 0x2173:
		case 0x2174:
		case 0x2175:
		case 0x2176:
		case 0x2177:
		case 0x2178:
		case 0x2179:
		case 0x217a:
		case 0x217b:
		case 0x217c:
		case 0x217d:
		case 0x217e:
		case 0x217f:
			IAPU.Executing = Settings.APUEnabled;
			IAPU.WaitCounter++;

			if (IAPU.Executing)
				return APU.OutPorts[Address & 3];

			CPU.BranchSkip = true;

			if ((Address & 3) < 2)
			{
				int32_t r = rand();

				if (r & 2)
				{
					if (r & 4)
						return (Address & 3) == 1 ? 0xaa : 0xbb;
					else
						return (r >> 3) & 0xff;
				}
			}
			else
			{
				int32_t r = rand();

				if (r & 2)
					return (r >> 3) & 0xff;
			}

			return Memory.FillRAM[Address];
		case 0x2180: /* WMDATA */
			if (CPU.InDMA)
				return ICPU.OpenBus;

			byte = Memory.RAM[PPU.WRAM++];
			PPU.WRAM &= 0x1ffff;
			return byte;
		default:
			return ICPU.OpenBus;
	}
}

void SetCPU(uint8_t byte, uint16_t Address)
{
	int32_t d;

	if (Address < 0x4200)
	{
		CPU.Cycles += ONE_CYCLE;

		switch (Address)
		{
			case 0x4016: /* Reset reading of old-style joypads */
				if ((byte & 1) && !(Memory.FillRAM[Address] & 1))
				{
					PPU.Joypad1ButtonReadPos = 0;
					PPU.Joypad2ButtonReadPos = 0;
					PPU.Joypad3ButtonReadPos = 0;
				}

				break;
			case 0x4017:
				return;
			default:
				break;
		}
	}
	else if ((Address & 0xff80) == 0x4300 && CPU.InDMA)
		return;

	switch (Address)
	{
		case 0x4200: /* NMI, V & H IRQ and joypad reading enable flags */
			if (byte & 0x20)
			{
				if (!PPU.VTimerEnabled)
				{
					PPU.VTimerEnabled = true;

					if (PPU.HTimerEnabled)
						UpdateHTimer();
					else if (PPU.IRQVBeamPos == CPU.V_Counter)
						SetIRQSource(PPU_V_BEAM_IRQ_SOURCE);
				}
			}
			else
				PPU.VTimerEnabled = false;

			if (byte & 0x10)
			{
				if (!PPU.HTimerEnabled)
				{
					PPU.HTimerEnabled = true;
					UpdateHTimer();
				}
			}
			else
			{
				/* No need to check for HTimer being disabled as the scanline
				   event trigger code won't trigger an H-IRQ unless its enabled. */
				PPU.HTimerEnabled  = false;
				PPU.HTimerPosition = Settings.H_Max + 1;
			}

			if (!(byte & 0x30))
				ClearIRQSource(PPU_V_BEAM_IRQ_SOURCE | PPU_H_BEAM_IRQ_SOURCE);

			if ((byte & 0x80) && !(Memory.FillRAM[0x4200] & 0x80) &&
			    (CPU.V_Counter >= PPU.ScreenHeight + FIRST_VISIBLE_LINE) &&
			    /* NMI can trigger during VBlank as long as NMI_read ($4210) wasn't cleared. */
			    /* Panic Bomberman clears the NMI pending flag @ scanline 230 before enabling
			     * NMIs again. The NMI routine crashes the CPU if it is called without the NMI
			     * pending flag being set... */
			    (Memory.FillRAM[0x4210] & 0x80) && !CPU.NMIActive)
			{
				CPU.Flags |= NMI_FLAG;
				CPU.NMIActive = true;
				CPU.NMICycleCount = CPU.Cycles + TWO_CYCLES;
			}

			break;
		case 0x4201:
			if ((byte & 0x80) == 0 && (Memory.FillRAM[0x4213] & 0x80) == 0x80)
				LatchCounters(1);

			Memory.FillRAM[0x4201] = Memory.FillRAM[0x4213] = byte;
			break;
		case 0x4202: /* Multiplier (for multiply) */
			break;
		case 0x4203: /* Multiplicand */
		{
			uint32_t res = Memory.FillRAM[0x4202] * byte;

		#ifdef MSB_FIRST
			Memory.FillRAM[0x4216] = (uint8_t) res;
			Memory.FillRAM[0x4217] = (uint8_t) (res >> 8);
		#else
			/* assume malloc'd memory is 2-byte aligned */
			*((uint16_t*) &Memory.FillRAM[0x4216]) = res;
		#endif

			break;
		}
		case 0x4204:
		case 0x4205: /* Low and high muliplier (for divide) */
			break;
		case 0x4206:
		{
		#ifdef MSB_FIRST
			uint16_t a = Memory.FillRAM[0x4204] + (Memory.FillRAM[0x4205] << 8);
		#else
			/* assume malloc'd memory is 2-byte aligned */
			uint16_t a = *((uint16_t*) &Memory.FillRAM[0x4204]);
		#endif

			uint16_t div = byte ? a / byte : 0xffff;
			uint16_t rem = byte ? a % byte : a;

		#ifdef MSB_FIRST
			Memory.FillRAM[0x4214] = (uint8_t) div;
			Memory.FillRAM[0x4215] = div >> 8;
			Memory.FillRAM[0x4216] = (uint8_t) rem;
			Memory.FillRAM[0x4217] = rem >> 8;
		#else
			/* assume malloc'd memory is 2-byte aligned */
			*((uint16_t*) &Memory.FillRAM[0x4214]) = div;
			*((uint16_t*) &Memory.FillRAM[0x4216]) = rem;
		#endif

			break;
		}
		case 0x4207:
			d               =  PPU.IRQHBeamPos;
			PPU.IRQHBeamPos = (PPU.IRQHBeamPos & 0xFF00) | byte;

			if (PPU.HTimerEnabled && PPU.IRQHBeamPos != d)
				UpdateHTimer();

			break;
		case 0x4208:
			d               =  PPU.IRQHBeamPos;
			PPU.IRQHBeamPos = (PPU.IRQHBeamPos & 0xFF) | ((byte & 1) << 8);

			if (PPU.HTimerEnabled && PPU.IRQHBeamPos != d)
				UpdateHTimer();

			break;
		case 0x4209:
			d               =  PPU.IRQVBeamPos;
			PPU.IRQVBeamPos = (PPU.IRQVBeamPos & 0xff00) | byte;

			if (PPU.VTimerEnabled && PPU.IRQVBeamPos != d)
			{
				if (PPU.HTimerEnabled)
					UpdateHTimer();
				else if (PPU.IRQVBeamPos == CPU.V_Counter)
					SetIRQSource(PPU_V_BEAM_IRQ_SOURCE);
			}

			break;
		case 0x420A:
			d               =  PPU.IRQVBeamPos;
			PPU.IRQVBeamPos = (PPU.IRQVBeamPos & 0xFF) | ((byte & 1) << 8);
			if (PPU.VTimerEnabled && PPU.IRQVBeamPos != d)
			{
				if (PPU.HTimerEnabled)
					UpdateHTimer();
				else if (PPU.IRQVBeamPos == CPU.V_Counter)
					SetIRQSource(PPU_V_BEAM_IRQ_SOURCE);
			}

			break;
		case 0x420B:
			if (byte & 0x01)
				DoDMA(0);

			if (byte & 0x02)
				DoDMA(1);

			if (byte & 0x04)
				DoDMA(2);

			if (byte & 0x08)
				DoDMA(3);

			if (byte & 0x10)
				DoDMA(4);

			if (byte & 0x20)
				DoDMA(5);

			if (byte & 0x40)
				DoDMA(6);

			if (byte & 0x80)
				DoDMA(7);

			break;
		case 0x420c:
			Memory.FillRAM[0x420c] = byte;
			IPPU.HDMA              = byte;
			break;
		case 0x420d: /* Cycle speed 0 - 2.68Mhz, 1 - 3.58Mhz (banks 0x80 +) */
			if ((byte & 1) == (Memory.FillRAM[0x420d] & 1))
				break;

			FixROMSpeed((byte & 1) ? ONE_CYCLE : SLOW_ONE_CYCLE);
			break;
		case 0x420e:
		case 0x420f: /* --->>> Unknown */
			break;
		case 0x4210: /* NMI ocurred flag (reset on read or write) */
			Memory.FillRAM[0x4210] = Model->_5A22;
			return;
		case 0x4211: /* IRQ ocurred flag (reset on read or write) */
			ClearIRQSource(PPU_V_BEAM_IRQ_SOURCE | PPU_H_BEAM_IRQ_SOURCE);
			break;
		case 0x4212: /* v-blank, h-blank and joypad being scanned flags (read-only) */
		case 0x4213: /* I/O Port (read-only) */
		case 0x4214:
		case 0x4215: /* Quotent of divide (read-only) */
		case 0x4216:
		case 0x4217: /* Multiply product (read-only) */
		case 0x4218:
		case 0x4219:
		case 0x421a:
		case 0x421b:
		case 0x421c:
		case 0x421d:
		case 0x421e:
		case 0x421f: /* Joypad values (read-only) */
			return;
		case 0x4300:
		case 0x4310:
		case 0x4320:
		case 0x4330:
		case 0x4340:
		case 0x4350:
		case 0x4360:
		case 0x4370:
			d                             = (Address >> 4) & 0x7;
			DMA[d].ReverseTransfer        = (bool) (byte & 0x80);
			DMA[d].HDMAIndirectAddressing = (bool) (byte & 0x40);
			Memory.FillRAM[Address | 0xf] = (byte & 0x20);
			DMA[d].AAddressDecrement      = (bool) (byte & 0x10);
			DMA[d].AAddressFixed          = (bool) (byte & 0x08);
			DMA[d].TransferMode           = (byte & 7);
			break;
		case 0x4301:
		case 0x4311:
		case 0x4321:
		case 0x4331:
		case 0x4341:
		case 0x4351:
		case 0x4361:
		case 0x4371:
			DMA[((Address >> 4) & 0x7)].BAddress = byte;
			break;
		case 0x4302:
		case 0x4312:
		case 0x4322:
		case 0x4332:
		case 0x4342:
		case 0x4352:
		case 0x4362:
		case 0x4372:
			d = (Address >> 4) & 0x7;
			DMA[d].AAddress &= 0xFF00;
			DMA[d].AAddress |= byte;
			break;
		case 0x4303:
		case 0x4313:
		case 0x4323:
		case 0x4333:
		case 0x4343:
		case 0x4353:
		case 0x4363:
		case 0x4373:
			d = (Address >> 4) & 0x7;
			DMA[d].AAddress &= 0xFF;
			DMA[d].AAddress |= byte << 8;
			break;
		case 0x4304:
		case 0x4314:
		case 0x4324:
		case 0x4334:
		case 0x4344:
		case 0x4354:
		case 0x4364:
		case 0x4374:
			DMA[((Address >> 4) & 0x7)].ABank       = byte;
			HDMAMemPointers[((Address >> 4) & 0x7)] = NULL;
			break;
		case 0x4305:
		case 0x4315:
		case 0x4325:
		case 0x4335:
		case 0x4345:
		case 0x4355:
		case 0x4365:
		case 0x4375:
			d = (Address >> 4) & 0x7;
			DMA[d].TransferBytes &= 0xff00;
			DMA[d].TransferBytes |= byte;
			DMA[d].IndirectAddress &= 0xff00;
			DMA[d].IndirectAddress |= byte;
			HDMAMemPointers[d] = NULL;
			break;
		case 0x4306:
		case 0x4316:
		case 0x4326:
		case 0x4336:
		case 0x4346:
		case 0x4356:
		case 0x4366:
		case 0x4376:
			d = (Address >> 4) & 0x7;
			DMA[d].TransferBytes &= 0xFF;
			DMA[d].TransferBytes |= byte << 8;
			DMA[d].IndirectAddress &= 0xff;
			DMA[d].IndirectAddress |= byte << 8;
			HDMAMemPointers[d] = NULL;
			break;
		case 0x4307:
		case 0x4317:
		case 0x4327:
		case 0x4337:
		case 0x4347:
		case 0x4357:
		case 0x4367:
		case 0x4377:
			DMA[d = ((Address >> 4) & 0x7)].IndirectBank = byte;
			HDMAMemPointers[d]                           = NULL;
			break;
		case 0x4308:
		case 0x4318:
		case 0x4328:
		case 0x4338:
		case 0x4348:
		case 0x4358:
		case 0x4368:
		case 0x4378:
			d = (Address >> 4) & 7;
			DMA[d].Address &= 0xff00;
			DMA[d].Address |= byte;
			HDMAMemPointers[d] = NULL;
			break;
		case 0x4309:
		case 0x4319:
		case 0x4329:
		case 0x4339:
		case 0x4349:
		case 0x4359:
		case 0x4369:
		case 0x4379:
			d = (Address >> 4) & 0x7;
			DMA[d].Address &= 0xff;
			DMA[d].Address |= byte << 8;
			HDMAMemPointers[d] = NULL;
			break;
		case 0x430A:
		case 0x431A:
		case 0x432A:
		case 0x433A:
		case 0x434A:
		case 0x435A:
		case 0x436A:
		case 0x437A:
			d                = (Address >> 4) & 0x7;
			DMA[d].LineCount = byte & 0x7f;
			DMA[d].Repeat    = !(byte & 0x80);
			break;
		case 0x430B:
		case 0x431B:
		case 0x432B:
		case 0x433B:
		case 0x434B:
		case 0x435B:
		case 0x436B:
		case 0x437B:
		case 0x430F:
		case 0x431F:
		case 0x432F:
		case 0x433F:
		case 0x434F:
		case 0x435F:
		case 0x436F:
		case 0x437F:
			Memory.FillRAM[Address | 0xf] = byte;
			break;
		case 0x4800:
		case 0x4801:
		case 0x4802:
		case 0x4803:
			if ((Settings.Chip & SPC7110) == SPC7110)
				SetSPC7110(byte, Address);

			break;
		case 0x4804:
		case 0x4805:
		case 0x4806:
		case 0x4807: /* These registers are used by both the S-DD1 and the SPC7110 */
			if ((Settings.Chip & SPC7110) == SPC7110)
				SetSPC7110(byte, Address);
			else
				SetSDD1MemoryMap(Address - 0x4804, byte & 7);

			break;
		case 0x4808:
		case 0x4809:
		case 0x480A:
		case 0x480B:
		case 0x480C:
		case 0x4810:
		case 0x4811:
		case 0x4812:
		case 0x4813:
		case 0x4814:
		case 0x4815:
		case 0x4816:
		case 0x4817:
		case 0x4818:
		case 0x481A:
		case 0x4820:
		case 0x4821:
		case 0x4822:
		case 0x4823:
		case 0x4824:
		case 0x4825:
		case 0x4826:
		case 0x4827:
		case 0x4828:
		case 0x4829:
		case 0x482A:
		case 0x482B:
		case 0x482C:
		case 0x482D:
		case 0x482E:
		case 0x482F:
		case 0x4830:
		case 0x4831:
		case 0x4832:
		case 0x4833:
		case 0x4834:
		case 0x4840:
		case 0x4841:
		case 0x4842:
			if ((Settings.Chip & SPC7110) == SPC7110)
				SetSPC7110(byte, Address);

			break;
	}

	Memory.FillRAM[Address] = byte;
}

uint8_t GetCPU(uint16_t Address)
{
	int32_t d;
	uint8_t byte;

	if (Address < 0x4200)
	{
		CPU.Cycles += ONE_CYCLE;

		switch (Address)
		{
			case 0x4016:
			{
				if (Memory.FillRAM[0x4016] & 1)
					return 0;

				if (PPU.Joypad1ButtonReadPos >= 16) /* Joypad 1 is enabled */
					return 1;

				return (IPPU.Joypads[0] >> (PPU.Joypad1ButtonReadPos++ ^ 15)) & 1;
			}
			case 0x4017:
			{
				if (Memory.FillRAM[0x4016] & 1)
				{
					if (IPPU.Controller == SNES_MULTIPLAYER5) /* MultiPlayer5 adaptor is only allowed to be plugged into port 2 */
						return 2;

					return 0;
				}

				if (IPPU.Controller == SNES_MULTIPLAYER5)
				{
					if (Memory.FillRAM[0x4201] & 0x80)
					{
						byte = ((IPPU.Joypads[1] >> (PPU.Joypad2ButtonReadPos ^ 15)) & 1) | (((IPPU.Joypads[2] >> (PPU.Joypad2ButtonReadPos ^ 15)) & 1) << 1);
						PPU.Joypad2ButtonReadPos++;
						return byte;
					}
					else
					{
						byte = ((IPPU.Joypads[3] >> (PPU.Joypad3ButtonReadPos ^ 15)) & 1) | (((IPPU.Joypads[4] >> (PPU.Joypad3ButtonReadPos ^ 15)) & 1) << 1);
						PPU.Joypad3ButtonReadPos++;
						return byte;
					}
				}
				else if (IPPU.Controller == SNES_JUSTIFIER || IPPU.Controller == SNES_JUSTIFIER_2)
				{
					uint8_t rv;
					rv = (1 & (justifiers >> in_bit));
					in_bit++;
					in_bit %= 32;
					return rv;
				}

				if (PPU.Joypad2ButtonReadPos >= 16) /* Joypad 2 is enabled */
					return 1;

				return (IPPU.Joypads[1] >> (PPU.Joypad2ButtonReadPos++ ^ 15)) & 1;
			}
			default:
				return ICPU.OpenBus;
		}
	}
	else
		switch (Address)
		{
			case 0x4200:
			case 0x4201:
			case 0x4202:
			case 0x4203:
			case 0x4204:
			case 0x4205:
			case 0x4206:
			case 0x4207:
			case 0x4208:
			case 0x4209:
			case 0x420a:
			case 0x420b:
			case 0x420c:
			case 0x420d:
			case 0x420e:
			case 0x420f:
				return ICPU.OpenBus;
			case 0x4210: /* RDNMI */
				CPU.WaitAddress = CPU.PCAtOpcodeStart;
				byte = Memory.FillRAM[0x4210];
				Memory.FillRAM[0x4210] = Model->_5A22;
				return (byte & 0x80) | (ICPU.OpenBus & 0x70) | Model->_5A22;
			case 0x4211: /* TIMEUP */
				byte = (CPU.IRQActive & (PPU_V_BEAM_IRQ_SOURCE | PPU_H_BEAM_IRQ_SOURCE)) ? 0x80 : 0;
				ClearIRQSource(PPU_V_BEAM_IRQ_SOURCE | PPU_H_BEAM_IRQ_SOURCE);
				return byte | (ICPU.OpenBus & 0x7f);
			case 0x4212: /* HVBJOY */
				CPU.WaitAddress = CPU.PCAtOpcodeStart;
				return REGISTER_4212() | (ICPU.OpenBus & 0x3e);
			case 0x4213: /* RDIO */
			case 0x4214: /* RDDIVL */
			case 0x4215: /* RDDIVH */
			case 0x4216: /* RDMPYL */
			case 0x4217: /* RDMPYH */
			case 0x4218: /* JOY1L */
			case 0x4219: /* JOY1H */
			case 0x421a: /* JOY2L */
			case 0x421b: /* JOY2H */
			case 0x421c: /* JOY3L */
			case 0x421d: /* JOY3H */
			case 0x421e: /* JOY4L */
			case 0x421f: /* JOY4H */
				return Memory.FillRAM[Address];
			case 0x4300:
			case 0x4310:
			case 0x4320:
			case 0x4330:
			case 0x4340:
			case 0x4350:
			case 0x4360:
			case 0x4370:
				d = (Address >> 4) & 0x7;
				return  (DMA[d].ReverseTransfer ? 0x80 : 0x00) | (DMA[d].HDMAIndirectAddressing ? 0x40 : 0x00) |
				        ((uint8_t) Memory.FillRAM [Address]) | (DMA[d].AAddressDecrement ? 0x10 : 0x00) |
				        (DMA[d].AAddressFixed ? 0x08 : 0x00) | (DMA[d].TransferMode & 7);
			case 0x4301:
			case 0x4311:
			case 0x4321:
			case 0x4331:
			case 0x4341:
			case 0x4351:
			case 0x4361:
			case 0x4371:
				return DMA[((Address >> 4) & 0x7)].BAddress;
			case 0x4302:
			case 0x4312:
			case 0x4322:
			case 0x4332:
			case 0x4342:
			case 0x4352:
			case 0x4362:
			case 0x4372:
				return DMA[((Address >> 4) & 0x7)].AAddress & 0xFF;
			case 0x4303:
			case 0x4313:
			case 0x4323:
			case 0x4333:
			case 0x4343:
			case 0x4353:
			case 0x4363:
			case 0x4373:
				return DMA[((Address >> 4) & 0x7)].AAddress >> 8;
			case 0x4304:
			case 0x4314:
			case 0x4324:
			case 0x4334:
			case 0x4344:
			case 0x4354:
			case 0x4364:
			case 0x4374:
				return DMA[((Address >> 4) & 0x7)].ABank;
			case 0x4305:
			case 0x4315:
			case 0x4325:
			case 0x4335:
			case 0x4345:
			case 0x4355:
			case 0x4365:
			case 0x4375:
				return DMA[((Address >> 4) & 0x7)].IndirectAddress & 0xff;
			case 0x4306:
			case 0x4316:
			case 0x4326:
			case 0x4336:
			case 0x4346:
			case 0x4356:
			case 0x4366:
			case 0x4376:
				return DMA[((Address >> 4) & 0x7)].IndirectAddress >> 8;
			case 0x4307:
			case 0x4317:
			case 0x4327:
			case 0x4337:
			case 0x4347:
			case 0x4357:
			case 0x4367:
			case 0x4377:
				return DMA[((Address >> 4) & 0x7)].IndirectBank;
			case 0x4308:
			case 0x4318:
			case 0x4328:
			case 0x4338:
			case 0x4348:
			case 0x4358:
			case 0x4368:
			case 0x4378:
				return DMA[((Address >> 4) & 0x7)].Address & 0xFF;
			case 0x4309:
			case 0x4319:
			case 0x4329:
			case 0x4339:
			case 0x4349:
			case 0x4359:
			case 0x4369:
			case 0x4379:
				return DMA[((Address >> 4) & 0x7)].Address >> 8;
			case 0x430A:
			case 0x431A:
			case 0x432A:
			case 0x433A:
			case 0x434A:
			case 0x435A:
			case 0x436A:
			case 0x437A:
				d = (Address >> 4) & 0x7;
				return DMA[d].LineCount ^ (DMA[d].Repeat ? 0x00 : 0x80);
			case 0x430B:
			case 0x431B:
			case 0x432B:
			case 0x433B:
			case 0x434B:
			case 0x435B:
			case 0x436B:
			case 0x437B:
			case 0x430F:
			case 0x431F:
			case 0x432F:
			case 0x433F:
			case 0x434F:
			case 0x435F:
			case 0x436F:
			case 0x437F:
				return (uint8_t) Memory.FillRAM[Address | 0xf];
			default:
				if ((Settings.Chip & SPC7110) == SPC7110 && Address >= 0x4800)
					return GetSPC7110(Address);

				if (Settings.Chip == S_DD1 && Address >= 0x4800 && Address <= 0x4807)
					return Memory.FillRAM[Address];

				return ICPU.OpenBus;
		}
}

void SoftResetPPU()
{
	uint8_t B;
	int32_t c;
	int32_t Sprite;
	PPU.BGMode               = 0;
	PPU.BG3Priority          = 0;
	PPU.Brightness           = 0;
	PPU.VMA.High             = false;
	PPU.VMA.Increment        = 1;
	PPU.VMA.Address          = 0;
	PPU.VMA.FullGraphicCount = 0;
	PPU.VMA.Shift            = 0;

	for (B = 0; B < 4; B++)
	{
		PPU.BG[B].SCBase              = 0;
		PPU.BG[B].VOffset             = 0;
		PPU.BG[B].HOffset             = 0;
		PPU.BG[B].BGSize              = 0;
		PPU.BG[B].NameBase            = 0;
		PPU.BG[B].SCSize              = 0;
		PPU.ClipWindowOverlapLogic[B] = CLIP_OR;
		PPU.ClipWindow1Enable[B]      = false;
		PPU.ClipWindow2Enable[B]      = false;
		PPU.ClipWindow1Inside[B]      = true;
		PPU.ClipWindow2Inside[B]      = true;
	}

	PPU.ClipWindowOverlapLogic[4] = PPU.ClipWindowOverlapLogic[5] = CLIP_OR;
	PPU.ClipWindow1Enable[4] = PPU.ClipWindow1Enable[5] = false;
	PPU.ClipWindow2Enable[4] = PPU.ClipWindow2Enable[5] = false;
	PPU.ClipWindow1Inside[4] = PPU.ClipWindow1Inside[5] = true;
	PPU.ClipWindow2Inside[4] = PPU.ClipWindow2Inside[5] = true;
	PPU.CGFLIP = false;

	for (c = 0; c < 256; c++)
	{
		IPPU.Red[c]   = (c & 7) << 2;
		IPPU.Green[c] = ((c >> 3) & 7) << 2;
		IPPU.Blue[c]  = ((c >> 6) & 2) << 3;
		PPU.CGDATA[c] = IPPU.Red[c] | (IPPU.Green[c] << 5) | (IPPU.Blue[c] << 10);
	}

	PPU.FirstSprite = 0;

	for (Sprite = 0; Sprite < 128; Sprite++)
	{
		PPU.OBJ[Sprite].HPos     = 0;
		PPU.OBJ[Sprite].VPos     = 0;
		PPU.OBJ[Sprite].VFlip    = 0;
		PPU.OBJ[Sprite].HFlip    = 0;
		PPU.OBJ[Sprite].Priority = 0;
		PPU.OBJ[Sprite].Palette  = 0;
		PPU.OBJ[Sprite].Name     = 0;
		PPU.OBJ[Sprite].Size     = 0;
	}

	PPU.OAMPriorityRotation                                         = 0;
	PPU.OAMWriteRegister                                            = 0;
	PPU.RangeTimeOver                                               = 0;
	PPU.OpenBus1                                                    = 0;
	PPU.OpenBus2                                                    = 0;
	PPU.OAMFlip                                                     = 0;
	PPU.OAMAddr                                                     = 0;
	PPU.IRQVBeamPos                                                 = 0;
	PPU.IRQHBeamPos                                                 = 0;
	PPU.VBeamPosLatched                                             = 0;
	PPU.HBeamPosLatched                                             = 0;
	PPU.HBeamFlip                                                   = 0;
	PPU.VBeamFlip                                                   = 0;
	PPU.MatrixA = PPU.MatrixB = PPU.MatrixC = PPU.MatrixD           = 0;
	PPU.CentreX = PPU.CentreY                                       = 0;
	PPU.CGADD                                                       = 0;
	PPU.FixedColourRed = PPU.FixedColourGreen = PPU.FixedColourBlue = 0;
	PPU.SavedOAMAddr                                                = 0;
	PPU.ScreenHeight                                                = SNES_HEIGHT;
	PPU.WRAM                                                        = 0;
	PPU.ForcedBlanking                                              = true;
	PPU.OBJSizeSelect                                               = 0;
	PPU.OBJNameSelect                                               = 0;
	PPU.OBJNameBase                                                 = 0;
	PPU.BGnxOFSbyte                                                 = 0;
	memset(PPU.OAMData, 0, 512 + 32);
	PPU.VTimerEnabled                                               = false;
	PPU.HTimerEnabled                                               = false;
	PPU.HTimerPosition                                              = Settings.H_Max + 1;
	PPU.Mosaic                                                      = 0;
	PPU.BGMosaic[0] = PPU.BGMosaic[1]                               = false;
	PPU.BGMosaic[2] = PPU.BGMosaic[3]                               = false;
	PPU.Mode7HFlip                                                  = false;
	PPU.Mode7VFlip                                                  = false;
	PPU.Mode7Repeat                                                 = 0;
	PPU.Window1Left                                                 = 1;
	PPU.Window1Right                                                = 0;
	PPU.Window2Left                                                 = 1;
	PPU.Window2Right                                                = 0;
	PPU.RecomputeClipWindows                                        = true;
	PPU.CGFLIPRead                                                  = false;
	PPU.Need16x8Multiply                                            = false;
	IPPU.ColorsChanged                                              = true;
	IPPU.HDMA                                                       = 0;
	IPPU.OBJChanged                                                 = true;
	IPPU.RenderThisFrame                                            = true;
	IPPU.DirectColourMapsNeedRebuild                                = true;
	IPPU.FrameCount                                                 = 0;
	memset(IPPU.TileCached[TILE_2BIT], 0, MAX_2BIT_TILES);
	memset(IPPU.TileCached[TILE_4BIT], 0, MAX_4BIT_TILES);
	memset(IPPU.TileCached[TILE_8BIT], 0, MAX_8BIT_TILES);
	IPPU.FirstVRAMRead                                              = false;
	IPPU.Interlace                                                  = false;
	IPPU.DoubleWidthPixels                                          = false;
	IPPU.HalfWidthPixels                                            = false;
	IPPU.DoubleHeightPixels                                         = false;
	IPPU.RenderedScreenWidth                                        = SNES_WIDTH;
	IPPU.RenderedScreenHeight                                       = SNES_HEIGHT;
	IPPU.XB                                                         = NULL;

	for (c = 0; c < 256; c++)
		IPPU.ScreenColors[c] = c;

	FixColourBrightness();
	IPPU.PreviousLine = IPPU.CurrentLine = 0;

	if (Settings.ControllerOption == 0)
		IPPU.Controller = SNES_MAX_CONTROLLER_OPTIONS - 1;
	else
		IPPU.Controller = Settings.ControllerOption - 1;

	NextController();

	for (c = 0; c < 2; c++)
		memset(&IPPU.Clip[c], 0, sizeof(ClipData));

	if (IPPU.Controller == SNES_MOUSE)
	{
		ProcessMouse(0);
		ProcessMouse(1);
	}

	for (c = 0; c < 0x8000; c += 0x100)
		memset(&Memory.FillRAM[c], c >> 8, 0x100);

	memset(&Memory.FillRAM[0x2100], 0, 0x100);
	memset(&Memory.FillRAM[0x4200], 0, 0x100);
	memset(&Memory.FillRAM[0x4000], 0, 0x100);
	memset(&Memory.FillRAM[0x1000], 0, 0x1000); /* For BS Suttehakkun 2... */
	Memory.FillRAM[0x4201] = Memory.FillRAM[0x4213] = 0xff;
}

void ResetPPU()
{
	int32_t c;
	SoftResetPPU();
	PPU.Joypad1ButtonReadPos                = 0;
	PPU.Joypad2ButtonReadPos                = 0;
	PPU.Joypad3ButtonReadPos                = 0;
	IPPU.Joypads[0] = IPPU.Joypads[1]       = IPPU.Joypads[2] = 0;
	IPPU.Joypads[3] = IPPU.Joypads[4]       = 0;
	IPPU.SuperScope                         = 0;
	IPPU.Mouse[0] = IPPU.Mouse[1]           = 0;
	IPPU.PrevMouseX[0] = IPPU.PrevMouseX[1] = 256 / 2;
	IPPU.PrevMouseY[0] = IPPU.PrevMouseY[1] = 224 / 2;
}

void ProcessMouse(int32_t which1)
{
	int32_t x, y;
	uint32_t buttons;

	if (IPPU.Controller == SNES_MOUSE && ReadMousePosition(which1, &x, &y, &buttons))
	{
		int32_t delta_x, delta_y;
		#define MOUSE_SIGNATURE 0x1
		IPPU.Mouse[which1] = MOUSE_SIGNATURE | ((buttons & 1) << 6) | ((buttons & 2) << 6);

		delta_x = x - IPPU.PrevMouseX[which1];
		delta_y = y - IPPU.PrevMouseY[which1];

		if (delta_x > 63)
		{
			delta_x = 63;
			IPPU.PrevMouseX[which1] += 63;
		}
		else if (delta_x < -63)
		{
			delta_x = -63;
			IPPU.PrevMouseX[which1] -= 63;
		}
		else
			IPPU.PrevMouseX[which1] = x;

		if (delta_y > 63)
		{
			delta_y = 63;
			IPPU.PrevMouseY[which1] += 63;
		}
		else if (delta_y < -63)
		{
			delta_y = -63;
			IPPU.PrevMouseY[which1] -= 63;
		}
		else
			IPPU.PrevMouseY[which1] = y;

		if (delta_x < 0)
		{
			delta_x = -delta_x;
			IPPU.Mouse[which1] |= (delta_x | 0x80) << 16;
		}
		else
			IPPU.Mouse[which1] |= delta_x << 16;

		if (delta_y < 0)
		{
			delta_y = -delta_y;
			IPPU.Mouse[which1] |= (delta_y | 0x80) << 24;
		}
		else
			IPPU.Mouse[which1] |= delta_y << 24;

		IPPU.Joypads[1] = IPPU.Mouse[which1];
	}
}

void ProcessSuperScope()
{
	int32_t  x, y;
	uint32_t buttons;

	if (IPPU.Controller != SNES_SUPERSCOPE || !ReadSuperScopePosition(&x, &y, &buttons))
		return;

	#define SUPERSCOPE_SIGNATURE 0x00ff
	uint32_t scope = SUPERSCOPE_SIGNATURE | ((buttons & 1) << (7 + 8)) | ((buttons & 2) << (5 + 8)) | ((buttons & 4) << (3 + 8)) | ((buttons & 8) << (1 + 8));

	if (Memory.FillRAM[0x4201] & 0x80)
	{
		x += 40;

		if (x > 295)
			x = 295;

		if (x < 40)
			x = 40;

		if (y > PPU.ScreenHeight - 1)
			y = PPU.ScreenHeight - 1;

		if (y < 0)
			y = 0;

		PPU.VBeamPosLatched = (uint16_t) (y + 1);
		PPU.HBeamPosLatched = (uint16_t) x;
		Memory.FillRAM[0x213F] |= 0x40 | Model->_5C78;
	}

	IPPU.Joypads[1] = scope;
}

void NextController()
{
	switch (IPPU.Controller)
	{
		case SNES_MULTIPLAYER5:
			IPPU.Controller = SNES_JOYPAD;
			break;
		case SNES_JOYPAD:
			IPPU.Controller = SNES_MOUSE;
			break;
		case SNES_MOUSE:
			IPPU.Controller = SNES_SUPERSCOPE;
			break;
		case SNES_SUPERSCOPE:
			IPPU.Controller = SNES_JUSTIFIER;
			break;
		case SNES_JUSTIFIER:
			IPPU.Controller = SNES_JUSTIFIER_2;
			break;
		case SNES_JUSTIFIER_2:
			IPPU.Controller = SNES_MULTIPLAYER5;
			break;
		default:
			IPPU.Controller = SNES_JOYPAD;
			break;
	}
}

void UpdateJustifiers()
{
	static bool last_p1;
	bool        offscreen;
	int32_t     x, y;
	uint32_t    buttons;
	in_bit     = 0;
	justifiers = 0xFFFF00AA;
	offscreen  = JustifierOffscreen();
	JustifierButtons(&justifiers);
	last_p1 = !last_p1;

	if (!last_p1)
		justifiers |= 0x1000;

	if (Memory.FillRAM[0x4201] & 0x80)
	{
		ReadSuperScopePosition(&x, &y, &buttons);

		x += 40;

		if (x > 295)
			x = 295;

		if (x < 40)
			x = 40;

		if (y > PPU.ScreenHeight - 1)
			y = PPU.ScreenHeight - 1;

		if (y < 0)
			y = 0;

		if (last_p1)
		{
			Memory.FillRAM[0x213F] = Model->_5C78;

			if (IPPU.Controller == SNES_JUSTIFIER_2)
			{
				if (!offscreen)
				{
					PPU.VBeamPosLatched = (uint16_t) (y + 1);
					PPU.HBeamPosLatched = (uint16_t) x;
					Memory.FillRAM[0x213F] |= 0x40 | Model->_5C78;
				}
			}
		}
		else
		{
			Memory.FillRAM[0x213F] = Model->_5C78;

			if (IPPU.Controller == SNES_JUSTIFIER) /* emulate player 1. */
			{
				if (!offscreen)
				{
					PPU.VBeamPosLatched = (uint16_t) (y + 1);
					PPU.HBeamPosLatched = (uint16_t) x;
					Memory.FillRAM[0x213F] |= 0x40 | Model->_5C78;
				}
			}
		}

		if (!offscreen) /* needs restructure */
		{
			if ((!last_p1 && IPPU.Controller == SNES_JUSTIFIER) || (last_p1 && IPPU.Controller == SNES_JUSTIFIER_2))
			{
				PPU.VBeamPosLatched = (uint16_t) (y + 1);
				PPU.HBeamPosLatched = (uint16_t) x;
				Memory.FillRAM[0x213F] |= 0x40 | Model->_5C78;
			}
			else
				Memory.FillRAM[0x213F] = Model->_5C78;
		}
		else
			Memory.FillRAM[0x213F] = Model->_5C78;
	}
}

void UpdateJoypads()
{
	uint32_t i;

	for (i = 0; i < 5; i++)
	{
		IPPU.Joypads[i] = ReadJoypad(i);

		if (IPPU.Joypads[i] &   SNES_LEFT_MASK)
			IPPU.Joypads[i] &= ~SNES_RIGHT_MASK;

		if (IPPU.Joypads[i] &   SNES_UP_MASK)
			IPPU.Joypads[i] &= ~SNES_DOWN_MASK;
	}

	if (IPPU.Controller == SNES_JOYPAD || IPPU.Controller == SNES_MULTIPLAYER5)
		for (i = 0; i < 5; i++)
			if (IPPU.Joypads[i])
				IPPU.Joypads[i] |= 0xffff0000;

	if (IPPU.Controller == SNES_MOUSE) /* Read mouse position if enabled */
		for (i = 0; i < 2; i++)
			ProcessMouse(i);

	if (IPPU.Controller == SNES_SUPERSCOPE) /* Read SuperScope if enabled */
		ProcessSuperScope();

	if (Memory.FillRAM[0x4200] & 1)
	{
		PPU.Joypad1ButtonReadPos = 16;

		if (Memory.FillRAM[0x4201] & 0x80)
		{
			PPU.Joypad2ButtonReadPos = 16;
			PPU.Joypad3ButtonReadPos = 0;
		}
		else
		{
			PPU.Joypad2ButtonReadPos = 0;
			PPU.Joypad3ButtonReadPos = 16;
		}

		Memory.FillRAM[0x4218] = (uint8_t)  IPPU.Joypads[0];
		Memory.FillRAM[0x4219] = (uint8_t) (IPPU.Joypads[0] >> 8);
		Memory.FillRAM[0x421a] = (uint8_t)  IPPU.Joypads[1];
		Memory.FillRAM[0x421b] = (uint8_t) (IPPU.Joypads[1] >> 8);

		if (Memory.FillRAM[0x4201] & 0x80)
		{
			Memory.FillRAM[0x421c] = (uint8_t)  IPPU.Joypads[0];
			Memory.FillRAM[0x421d] = (uint8_t) (IPPU.Joypads[0] >> 8);
			Memory.FillRAM[0x421e] = (uint8_t)  IPPU.Joypads[2];
			Memory.FillRAM[0x421f] = (uint8_t) (IPPU.Joypads[2] >> 8);
		}
		else
		{
			Memory.FillRAM[0x421c] = (uint8_t) IPPU.Joypads[3];
			Memory.FillRAM[0x421d] = (uint8_t) (IPPU.Joypads[3] >> 8);
			Memory.FillRAM[0x421e] = (uint8_t) IPPU.Joypads[4];
			Memory.FillRAM[0x421f] = (uint8_t) (IPPU.Joypads[4] >> 8);
		}
	}

	if (IPPU.Controller == SNES_JUSTIFIER || IPPU.Controller == SNES_JUSTIFIER_2)
	{
		Memory.FillRAM[0x421a] = 0x0E;
		Memory.FillRAM[0x421b] = 0;
		UpdateJustifiers();
	}
}
