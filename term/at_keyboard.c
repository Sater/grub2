/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2007,2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/at_keyboard.h>
#include <grub/cpu/at_keyboard.h>
#include <grub/cpu/io.h>
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/keyboard_layouts.h>

static short at_keyboard_status = 0;
static int e0_received = 0;
static int f0_received = 0;
static int pending_key = -1;

static grub_uint8_t led_status;

#define KEYBOARD_LED_SCROLL		(1 << 0)
#define KEYBOARD_LED_NUM		(1 << 1)
#define KEYBOARD_LED_CAPS		(1 << 2)

static grub_uint8_t grub_keyboard_controller_orig;
static grub_uint8_t grub_keyboard_orig_set;
static grub_uint8_t current_set; 

static const grub_uint8_t set1_mapping[128] =
  {
    /* 0x00 */ 0 /* Unused  */,               GRUB_KEYBOARD_KEY_ESCAPE, 
    /* 0x02 */ GRUB_KEYBOARD_KEY_1,           GRUB_KEYBOARD_KEY_2, 
    /* 0x04 */ GRUB_KEYBOARD_KEY_3,           GRUB_KEYBOARD_KEY_4, 
    /* 0x06 */ GRUB_KEYBOARD_KEY_5,           GRUB_KEYBOARD_KEY_6, 
    /* 0x08 */ GRUB_KEYBOARD_KEY_7,           GRUB_KEYBOARD_KEY_8, 
    /* 0x0a */ GRUB_KEYBOARD_KEY_9,           GRUB_KEYBOARD_KEY_0, 
    /* 0x0c */ GRUB_KEYBOARD_KEY_DASH,        GRUB_KEYBOARD_KEY_EQUAL, 
    /* 0x0e */ GRUB_KEYBOARD_KEY_BACKSPACE,   GRUB_KEYBOARD_KEY_TAB, 
    /* 0x10 */ GRUB_KEYBOARD_KEY_Q,           GRUB_KEYBOARD_KEY_W, 
    /* 0x12 */ GRUB_KEYBOARD_KEY_E,           GRUB_KEYBOARD_KEY_R, 
    /* 0x14 */ GRUB_KEYBOARD_KEY_T,           GRUB_KEYBOARD_KEY_Y, 
    /* 0x16 */ GRUB_KEYBOARD_KEY_U,           GRUB_KEYBOARD_KEY_I, 
    /* 0x18 */ GRUB_KEYBOARD_KEY_O,           GRUB_KEYBOARD_KEY_P, 
    /* 0x1a */ GRUB_KEYBOARD_KEY_LBRACKET,    GRUB_KEYBOARD_KEY_RBRACKET, 
    /* 0x1c */ GRUB_KEYBOARD_KEY_ENTER,       GRUB_KEYBOARD_KEY_LEFT_CTRL, 
    /* 0x1e */ GRUB_KEYBOARD_KEY_A,           GRUB_KEYBOARD_KEY_S, 
    /* 0x20 */ GRUB_KEYBOARD_KEY_D,           GRUB_KEYBOARD_KEY_F, 
    /* 0x22 */ GRUB_KEYBOARD_KEY_G,           GRUB_KEYBOARD_KEY_H, 
    /* 0x24 */ GRUB_KEYBOARD_KEY_J,           GRUB_KEYBOARD_KEY_K, 
    /* 0x26 */ GRUB_KEYBOARD_KEY_L,           GRUB_KEYBOARD_KEY_SEMICOLON, 
    /* 0x28 */ GRUB_KEYBOARD_KEY_DQUOTE,      GRUB_KEYBOARD_KEY_RQUOTE, 
    /* 0x2a */ GRUB_KEYBOARD_KEY_LEFT_SHIFT,  GRUB_KEYBOARD_KEY_BACKSLASH, 
    /* 0x2c */ GRUB_KEYBOARD_KEY_Z,           GRUB_KEYBOARD_KEY_X, 
    /* 0x2e */ GRUB_KEYBOARD_KEY_C,           GRUB_KEYBOARD_KEY_V, 
    /* 0x30 */ GRUB_KEYBOARD_KEY_B,           GRUB_KEYBOARD_KEY_N, 
    /* 0x32 */ GRUB_KEYBOARD_KEY_M,           GRUB_KEYBOARD_KEY_COMMA, 
    /* 0x34 */ GRUB_KEYBOARD_KEY_DOT,         GRUB_KEYBOARD_KEY_SLASH, 
    /* 0x36 */ GRUB_KEYBOARD_KEY_RIGHT_SHIFT, GRUB_KEYBOARD_KEY_NUMMUL, 
    /* 0x38 */ GRUB_KEYBOARD_KEY_LEFT_ALT,    GRUB_KEYBOARD_KEY_SPACE, 
    /* 0x3a */ GRUB_KEYBOARD_KEY_CAPS_LOCK,   GRUB_KEYBOARD_KEY_F1, 
    /* 0x3c */ GRUB_KEYBOARD_KEY_F2,          GRUB_KEYBOARD_KEY_F3, 
    /* 0x3e */ GRUB_KEYBOARD_KEY_F4,          GRUB_KEYBOARD_KEY_F5, 
    /* 0x40 */ GRUB_KEYBOARD_KEY_F6,          GRUB_KEYBOARD_KEY_F7, 
    /* 0x42 */ GRUB_KEYBOARD_KEY_F8,          GRUB_KEYBOARD_KEY_F9, 
    /* 0x44 */ GRUB_KEYBOARD_KEY_F10,         GRUB_KEYBOARD_KEY_NUM_LOCK, 
    /* 0x46 */ GRUB_KEYBOARD_KEY_SCROLL_LOCK, GRUB_KEYBOARD_KEY_NUM7, 
    /* 0x48 */ GRUB_KEYBOARD_KEY_NUM8,        GRUB_KEYBOARD_KEY_NUM9, 
    /* 0x4a */ GRUB_KEYBOARD_KEY_NUMMINUS,    GRUB_KEYBOARD_KEY_NUM4, 
    /* 0x4c */ GRUB_KEYBOARD_KEY_NUM5,        GRUB_KEYBOARD_KEY_NUM6, 
    /* 0x4e */ GRUB_KEYBOARD_KEY_NUMPLUS,     GRUB_KEYBOARD_KEY_NUM1, 
    /* 0x50 */ GRUB_KEYBOARD_KEY_NUM2,        GRUB_KEYBOARD_KEY_NUM3, 
    /* 0x52 */ GRUB_KEYBOARD_KEY_NUMDOT,      GRUB_KEYBOARD_KEY_NUMDOT, 
    /* 0x54 */ 0,                             0, 
    /* 0x56 */ GRUB_KEYBOARD_KEY_102ND,       GRUB_KEYBOARD_KEY_F11, 
    /* 0x58 */ GRUB_KEYBOARD_KEY_F12,         0,
    /* 0x5a */ 0,                             0,
    /* 0x5c */ 0,                             0,
    /* 0x5e */ 0,                             0,
    /* 0x60 */ 0,                             0,
    /* 0x62 */ 0,                             0,
    /* OLPC keys. Just mapped to normal keys.  */
    /* 0x64 */ 0,                             GRUB_KEYBOARD_KEY_UP,
    /* 0x66 */ GRUB_KEYBOARD_KEY_DOWN,        GRUB_KEYBOARD_KEY_LEFT,
    /* 0x68 */ GRUB_KEYBOARD_KEY_RIGHT
  };

static const struct
{
  grub_uint8_t from, to;
} set1_e0_mapping[] = 
  {
    {0x1c, GRUB_KEYBOARD_KEY_NUMENTER},
    {0x1d, GRUB_KEYBOARD_KEY_RIGHT_CTRL},
    {0x35, GRUB_KEYBOARD_KEY_NUMSLASH }, 
    {0x38, GRUB_KEYBOARD_KEY_RIGHT_ALT},
    {0x47, GRUB_KEYBOARD_KEY_HOME}, 
    {0x48, GRUB_KEYBOARD_KEY_UP},
    {0x49, GRUB_KEYBOARD_KEY_NPAGE},
    {0x4b, GRUB_KEYBOARD_KEY_LEFT},
    {0x4d, GRUB_KEYBOARD_KEY_RIGHT},
    {0x4f, GRUB_KEYBOARD_KEY_END}, 
    {0x50, GRUB_KEYBOARD_KEY_DOWN},
    {0x51, GRUB_KEYBOARD_KEY_PPAGE}, 
    {0x52, GRUB_KEYBOARD_KEY_INSERT},
    {0x53, GRUB_KEYBOARD_KEY_DELETE}, 
  };

static const grub_uint8_t set2_mapping[256] =
  {
    /* 0x00 */ 0,                             GRUB_KEYBOARD_KEY_F9,
    /* 0x02 */ 0,                             GRUB_KEYBOARD_KEY_F5,
    /* 0x04 */ GRUB_KEYBOARD_KEY_F3,          GRUB_KEYBOARD_KEY_F1,
    /* 0x06 */ GRUB_KEYBOARD_KEY_F2,          GRUB_KEYBOARD_KEY_F12,
    /* 0x08 */ 0,                             GRUB_KEYBOARD_KEY_F10,
    /* 0x0a */ GRUB_KEYBOARD_KEY_F8,          GRUB_KEYBOARD_KEY_F6,
    /* 0x0c */ GRUB_KEYBOARD_KEY_F4,          GRUB_KEYBOARD_KEY_TAB,
    /* 0x0e */ GRUB_KEYBOARD_KEY_RQUOTE,      0,
    /* 0x10 */ 0,                             GRUB_KEYBOARD_KEY_LEFT_ALT,
    /* 0x12 */ GRUB_KEYBOARD_KEY_LEFT_SHIFT,  0,
    /* 0x14 */ GRUB_KEYBOARD_KEY_LEFT_CTRL,   GRUB_KEYBOARD_KEY_Q,
    /* 0x16 */ GRUB_KEYBOARD_KEY_1,           0,
    /* 0x18 */ 0,                             0,
    /* 0x1a */ GRUB_KEYBOARD_KEY_Z,           GRUB_KEYBOARD_KEY_S,
    /* 0x1c */ GRUB_KEYBOARD_KEY_A,           GRUB_KEYBOARD_KEY_W,
    /* 0x1e */ GRUB_KEYBOARD_KEY_2,           0,
    /* 0x20 */ 0,                             GRUB_KEYBOARD_KEY_C,
    /* 0x22 */ GRUB_KEYBOARD_KEY_X,           GRUB_KEYBOARD_KEY_D,
    /* 0x24 */ GRUB_KEYBOARD_KEY_E,           GRUB_KEYBOARD_KEY_4,
    /* 0x26 */ GRUB_KEYBOARD_KEY_3,           0,
    /* 0x28 */ 0,                             GRUB_KEYBOARD_KEY_SPACE,
    /* 0x2a */ GRUB_KEYBOARD_KEY_V,           GRUB_KEYBOARD_KEY_F,
    /* 0x2c */ GRUB_KEYBOARD_KEY_T,           GRUB_KEYBOARD_KEY_R,
    /* 0x2e */ GRUB_KEYBOARD_KEY_5,           0,
    /* 0x30 */ 0,                             GRUB_KEYBOARD_KEY_N,
    /* 0x32 */ GRUB_KEYBOARD_KEY_B,           GRUB_KEYBOARD_KEY_H,
    /* 0x34 */ GRUB_KEYBOARD_KEY_G,           GRUB_KEYBOARD_KEY_Y,
    /* 0x36 */ GRUB_KEYBOARD_KEY_6,           0,
    /* 0x38 */ 0,                             0,
    /* 0x3a */ GRUB_KEYBOARD_KEY_M,           GRUB_KEYBOARD_KEY_J,
    /* 0x3c */ GRUB_KEYBOARD_KEY_U,           GRUB_KEYBOARD_KEY_7,
    /* 0x3e */ GRUB_KEYBOARD_KEY_8,           0,
    /* 0x40 */ 0,                             GRUB_KEYBOARD_KEY_DOT,
    /* 0x42 */ GRUB_KEYBOARD_KEY_K,           GRUB_KEYBOARD_KEY_I,
    /* 0x44 */ GRUB_KEYBOARD_KEY_O,           GRUB_KEYBOARD_KEY_0,
    /* 0x46 */ GRUB_KEYBOARD_KEY_9,           0,
    /* 0x48 */ 0,                             GRUB_KEYBOARD_KEY_COMMA,
    /* 0x4a */ GRUB_KEYBOARD_KEY_SLASH,       GRUB_KEYBOARD_KEY_L,
    /* 0x4c */ GRUB_KEYBOARD_KEY_SEMICOLON,   GRUB_KEYBOARD_KEY_P,
    /* 0x4e */ GRUB_KEYBOARD_KEY_DASH,        0,
    /* 0x50 */ 0,                             0,
    /* 0x52 */ GRUB_KEYBOARD_KEY_DQUOTE,      0,
    /* 0x54 */ GRUB_KEYBOARD_KEY_LBRACKET,    GRUB_KEYBOARD_KEY_EQUAL,
    /* 0x56 */ 0,                             0,
    /* 0x58 */ GRUB_KEYBOARD_KEY_CAPS_LOCK,   GRUB_KEYBOARD_KEY_RIGHT_SHIFT,
    /* 0x5a */ GRUB_KEYBOARD_KEY_ENTER,       GRUB_KEYBOARD_KEY_RBRACKET,
    /* 0x5c */ 0,                             GRUB_KEYBOARD_KEY_BACKSLASH,
    /* 0x5e */ 0,                             0,
    /* 0x60 */ 0,                             GRUB_KEYBOARD_KEY_102ND,
    /* 0x62 */ 0,                             0,
    /* 0x64 */ 0,                             0,
    /* 0x66 */ GRUB_KEYBOARD_KEY_BACKSPACE,   0,
    /* 0x68 */ 0,                             GRUB_KEYBOARD_KEY_NUM1,
    /* 0x6a */ 0,                             GRUB_KEYBOARD_KEY_NUM4,
    /* 0x6c */ GRUB_KEYBOARD_KEY_NUM7,        0,
    /* 0x6e */ 0,                             0,
    /* 0x70 */ GRUB_KEYBOARD_KEY_NUMDOT,      GRUB_KEYBOARD_KEY_NUM0,
    /* 0x72 */ GRUB_KEYBOARD_KEY_NUM2,        GRUB_KEYBOARD_KEY_NUM5,
    /* 0x74 */ GRUB_KEYBOARD_KEY_NUM6,        GRUB_KEYBOARD_KEY_NUM8,
    /* 0x76 */ GRUB_KEYBOARD_KEY_ESCAPE,      GRUB_KEYBOARD_KEY_NUM_LOCK,
    /* 0x78 */ GRUB_KEYBOARD_KEY_F11,         GRUB_KEYBOARD_KEY_NUMPLUS,
    /* 0x7a */ GRUB_KEYBOARD_KEY_NUM3,        GRUB_KEYBOARD_KEY_NUMMINUS,
    /* 0x7c */ GRUB_KEYBOARD_KEY_NUMMUL,      GRUB_KEYBOARD_KEY_NUM9,
    /* 0x7e */ GRUB_KEYBOARD_KEY_SCROLL_LOCK, 0,
    /* 0x80 */ 0,                             0, 
    /* 0x82 */ 0,                             GRUB_KEYBOARD_KEY_F7,
  };

static const struct
{
  grub_uint8_t from, to;
} set2_e0_mapping[] = 
  {
    {0x11, GRUB_KEYBOARD_KEY_RIGHT_ALT},
    {0x14, GRUB_KEYBOARD_KEY_RIGHT_CTRL},
    {0x4a, GRUB_KEYBOARD_KEY_NUMSLASH},
    {0x5a, GRUB_KEYBOARD_KEY_NUMENTER},
    {0x69, GRUB_KEYBOARD_KEY_END},
    {0x6b, GRUB_KEYBOARD_KEY_LEFT},
    {0x6c, GRUB_KEYBOARD_KEY_HOME},
    {0x70, GRUB_KEYBOARD_KEY_INSERT},
    {0x71, GRUB_KEYBOARD_KEY_DELETE},
    {0x72, GRUB_KEYBOARD_KEY_DOWN},
    {0x74, GRUB_KEYBOARD_KEY_RIGHT},
    {0x75, GRUB_KEYBOARD_KEY_UP},
    {0x7a, GRUB_KEYBOARD_KEY_NPAGE},
    {0x7d, GRUB_KEYBOARD_KEY_PPAGE},
  };

static void
keyboard_controller_wait_until_ready (void)
{
  while (! KEYBOARD_COMMAND_ISREADY (grub_inb (KEYBOARD_REG_STATUS)));
}

static void
grub_keyboard_controller_write (grub_uint8_t c)
{
  keyboard_controller_wait_until_ready ();
  grub_outb (KEYBOARD_COMMAND_WRITE, KEYBOARD_REG_STATUS);
  grub_outb (c, KEYBOARD_REG_DATA);
}

static grub_uint8_t
query_mode (int mode)
{
  keyboard_controller_wait_until_ready ();
  grub_outb (0xf0, KEYBOARD_REG_DATA);
  keyboard_controller_wait_until_ready ();
  grub_inb (KEYBOARD_REG_DATA);
  keyboard_controller_wait_until_ready ();
  grub_outb (mode, KEYBOARD_REG_DATA);

  keyboard_controller_wait_until_ready ();

  return grub_inb (KEYBOARD_REG_DATA);
}

/* QEMU translates the set even in no-translate mode.  */
static inline int
recover_mode (grub_uint8_t report)
{
  if (report == 0x43 || report == 1)
    return 1;
  if (report == 0x41 || report == 2)
    return 2;
  if (report == 0x3f || report == 3)
    return 3;
  return -1;
}

static void
set_scancodes (void)
{
  grub_keyboard_controller_write (grub_keyboard_controller_orig
				  & ~KEYBOARD_AT_TRANSLATE);
  grub_keyboard_orig_set = recover_mode (query_mode (0));

  query_mode (2);
  current_set = query_mode (0);
  current_set = recover_mode (current_set);
  if (current_set == 2)
    return;

  query_mode (1);
  current_set = query_mode (0);
  current_set = recover_mode (current_set);
  if (current_set == 1)
    return;
  grub_printf ("No supported scancode set found\n");
}

static grub_uint8_t
grub_keyboard_controller_read (void)
{
  keyboard_controller_wait_until_ready ();
  grub_outb (KEYBOARD_COMMAND_READ, KEYBOARD_REG_STATUS);
  return grub_inb (KEYBOARD_REG_DATA);
}

static void
keyboard_controller_led (grub_uint8_t leds)
{
  keyboard_controller_wait_until_ready ();
  grub_outb (0xed, KEYBOARD_REG_DATA);
  keyboard_controller_wait_until_ready ();
  grub_outb (leds & 0x7, KEYBOARD_REG_DATA);
}

static int
fetch_key (int *is_break)
{
  int was_ext = 0;
  grub_uint8_t at_key;
  int ret = 0;

  if (! KEYBOARD_ISREADY (grub_inb (KEYBOARD_REG_STATUS)))
    return -1;
  at_key = grub_inb (KEYBOARD_REG_DATA);
  if (at_key == 0xe0)
    {
      e0_received = 1;
      return -1;
    }

  was_ext = e0_received;
  e0_received = 0;

  switch (current_set)
    {
    case 1:
      *is_break = !!(at_key & 0x80);
      if (!was_ext)
	ret = set1_mapping[at_key & 0x7f];
      else
	{
	  unsigned i;
	  for (i = 0; i < ARRAY_SIZE (set1_e0_mapping); i++)
	    if (set1_e0_mapping[i].from == (at_key & 0x80))
	      {
		ret = set1_e0_mapping[i].to;
		break;
	      }
	}
      break;
    case 2:
      if (at_key == 0xf0)
	{
	  f0_received = 1;
	  return -1;
	}
      *is_break = f0_received;
      f0_received = 0;
      if (!was_ext)
	ret = set2_mapping[at_key];
      else
	{
	  unsigned i;
	  for (i = 0; i < ARRAY_SIZE (set1_e0_mapping); i++)
	    if (set1_e0_mapping[i].from == (at_key & 0x80))
	      {
		ret = set1_e0_mapping[i].to;
		break;
	      }
	}	
      break;
    default:
      return -1;
    }
  if (!ret)
    {
      grub_printf ("Unknown key 0x%02x from set %d\n\n", at_key, current_set);
      return -1;
    }
  return ret;
}

/* FIXME: This should become an interrupt service routine.  For now
   it's just used to catch events from control keys.  */
static int
grub_keyboard_isr (grub_keyboard_key_t key, int is_break)
{
  if (!is_break)
    switch (key)
      {
      case GRUB_KEYBOARD_KEY_LEFT_SHIFT:
	at_keyboard_status |= GRUB_TERM_STATUS_LSHIFT;
	return 1;
      case GRUB_KEYBOARD_KEY_RIGHT_SHIFT:
	at_keyboard_status |= GRUB_TERM_STATUS_RSHIFT;
	return 1;
      case GRUB_KEYBOARD_KEY_LEFT_CTRL:
	at_keyboard_status |= GRUB_TERM_STATUS_LCTRL;
	return 1;
      case GRUB_KEYBOARD_KEY_RIGHT_CTRL:
	at_keyboard_status |= GRUB_TERM_STATUS_RCTRL;
	return 1;
      case GRUB_KEYBOARD_KEY_RIGHT_ALT:
	at_keyboard_status |= GRUB_TERM_STATUS_RALT;
	return 1;
      case GRUB_KEYBOARD_KEY_LEFT_ALT:
	at_keyboard_status |= GRUB_TERM_STATUS_LALT;
	return 1;
      default:
	return 0;
      }
  else
    switch (KEYBOARD_SCANCODE (key))
      {
      case GRUB_KEYBOARD_KEY_LEFT_SHIFT:
	at_keyboard_status &= ~GRUB_TERM_STATUS_LSHIFT;
	return 1;
      case GRUB_KEYBOARD_KEY_RIGHT_SHIFT:
	at_keyboard_status &= ~GRUB_TERM_STATUS_RSHIFT;
	return 1;
      case GRUB_KEYBOARD_KEY_LEFT_CTRL:
	at_keyboard_status &= ~GRUB_TERM_STATUS_LCTRL;
	return 1;
      case GRUB_KEYBOARD_KEY_RIGHT_CTRL:
	at_keyboard_status &= ~GRUB_TERM_STATUS_RCTRL;
	return 1;
      case GRUB_KEYBOARD_KEY_RIGHT_ALT:
	at_keyboard_status &= ~GRUB_TERM_STATUS_RALT;
	return 1;
      case GRUB_KEYBOARD_KEY_LEFT_ALT:
	at_keyboard_status &= ~GRUB_TERM_STATUS_LALT;
	return 1;
      default:
	return 0;
      }
}

/* If there is a raw key pending, return it; otherwise return -1.  */
static int
grub_keyboard_getkey (void)
{
  int key;
  int is_break;

  key = fetch_key (&is_break);
  if (key == -1)
    return -1;

  if (grub_keyboard_isr (key, is_break))
    return -1;
  if (is_break)
    return -1;
  return key;
}

/* If there is a character pending, return it; otherwise return -1.  */
static int
grub_at_keyboard_getkey_noblock (void)
{
  int code;
  code = grub_keyboard_getkey ();
  if (code == -1)
    return -1;
#ifdef DEBUG_AT_KEYBOARD
  grub_dprintf ("atkeyb", "Detected key 0x%x\n", key);
#endif
  switch (code)
    {
      case GRUB_KEYBOARD_KEY_CAPS_LOCK:
	at_keyboard_status ^= GRUB_TERM_STATUS_CAPS;
	led_status ^= KEYBOARD_LED_CAPS;
	keyboard_controller_led (led_status);

#ifdef DEBUG_AT_KEYBOARD
	grub_dprintf ("atkeyb", "caps_lock = %d\n", !!(at_keyboard_status & KEYBOARD_STATUS_CAPS_LOCK));
#endif
	return -1;
      case GRUB_KEYBOARD_KEY_NUM_LOCK:
	at_keyboard_status ^= GRUB_TERM_STATUS_NUM;
	led_status ^= KEYBOARD_LED_NUM;
	keyboard_controller_led (led_status);

#ifdef DEBUG_AT_KEYBOARD
	grub_dprintf ("atkeyb", "num_lock = %d\n", !!(at_keyboard_status & KEYBOARD_STATUS_NUM_LOCK));
#endif
	return -1;
      case GRUB_KEYBOARD_KEY_SCROLL_LOCK:
	at_keyboard_status ^= GRUB_TERM_STATUS_SCROLL;
	led_status ^= KEYBOARD_LED_SCROLL;
	keyboard_controller_led (led_status);
	return -1;
      default:
	return grub_term_map_key (code, at_keyboard_status);
    }
}

static int
grub_at_keyboard_checkkey (struct grub_term_input *term __attribute__ ((unused)))
{
  if (pending_key != -1)
    return 1;

  pending_key = grub_at_keyboard_getkey_noblock ();

  if (pending_key != -1)
    return 1;

  return -1;
}

static int
grub_at_keyboard_getkey (struct grub_term_input *term __attribute__ ((unused)))
{
  int key;
  if (pending_key != -1)
    {
      key = pending_key;
      pending_key = -1;
      return key;
    }
  do
    {
      key = grub_at_keyboard_getkey_noblock ();
    } while (key == -1);
  return key;
}

static grub_err_t
grub_keyboard_controller_init (struct grub_term_input *term __attribute__ ((unused)))
{
  pending_key = -1;
  at_keyboard_status = 0;
  grub_keyboard_controller_orig = grub_keyboard_controller_read ();
  set_scancodes ();
  keyboard_controller_led (led_status);
  /* Drain input buffer. */
  while (KEYBOARD_ISREADY (grub_inb (KEYBOARD_REG_STATUS)))
    grub_inb (KEYBOARD_REG_DATA);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_keyboard_controller_fini (struct grub_term_input *term __attribute__ ((unused)))
{
  query_mode (grub_keyboard_orig_set);
  grub_keyboard_controller_write (grub_keyboard_controller_orig);
  return GRUB_ERR_NONE;
}

static struct grub_term_input grub_at_keyboard_term =
  {
    .name = "at_keyboard",
    .init = grub_keyboard_controller_init,
    .fini = grub_keyboard_controller_fini,
    .checkkey = grub_at_keyboard_checkkey,
    .getkey = grub_at_keyboard_getkey
  };

GRUB_MOD_INIT(at_keyboard)
{
  grub_term_register_input ("at_keyboard", &grub_at_keyboard_term);
}

GRUB_MOD_FINI(at_keyboard)
{
  grub_term_unregister_input (&grub_at_keyboard_term);
}
