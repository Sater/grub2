/* list.c - grub list function */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#include <grub/list.h>
#include <grub/misc.h>

void
grub_list_push (grub_list_t *head, grub_list_t item)
{
  item->next = *head;
  *head = item;
}

void *
grub_list_pop (grub_list_t *head)
{
  grub_list_t item;

  item = *head;
  if (item)
    *head = item->next;

  return item;
}

void
grub_list_remove (grub_list_t *head, grub_list_t item)
{
  grub_list_t *p, q;

  for (p = head, q = *p; q; p = &(q->next), q = q->next)
    if (q == item)
      {
	*p = q->next;
	break;
      }
}

void
grub_list_iterate (grub_list_t head, grub_list_hook_t hook)
{
  grub_list_t p;

  for (p = head; p; p = p->next)
    if (hook (p))
      break;
}

void *
grub_named_list_find (grub_named_list_t head, const char *name)
{
  grub_named_list_t result = 0;

  auto int list_find (grub_named_list_t item);
  int list_find (grub_named_list_t item)
    {
      if (! grub_strcmp (item->name, name))
	{
	  result = item;
	  return 1;
	}

      return 0;
    }

  grub_list_iterate (GRUB_AS_LIST (head), (grub_list_hook_t) list_find);
  return result;
}