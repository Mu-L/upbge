/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 */

#include <cctype>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_text_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"
#include "BKE_text.h"
#include "BKE_text_suggestions.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_text.hh"
#include "ED_undo.hh"

#include "text_format.hh"
#include "text_intern.hh" /* Own include. */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

bool space_text_do_suggest_select(SpaceText *st, const ARegion *region, const int mval[2])
{
  const int lheight = TXT_LINE_HEIGHT(st);
  SuggItem *item, *first, *last /* , *sel */ /* UNUSED. */;
  TextLine *tmp;
  int l, x, y, w, h, i;
  int tgti, *top;

  if (!st->text) {
    return false;
  }
  if (!texttool_text_is_active(st->text)) {
    return false;
  }

  first = texttool_suggest_first();
  last = texttool_suggest_last();
  // sel = texttool_suggest_selected(); /* UNUSED. */
  top = texttool_suggest_top();

  if (!last || !first) {
    return false;
  }

  /* Count the visible lines to the cursor. */
  for (tmp = st->text->curl, l = -st->top; tmp; tmp = tmp->prev, l++) {
    /* Pass. */
  }
  if (l < 0) {
    return false;
  }

  space_text_update_character_width(st);

  x = TXT_BODY_LEFT(st) + (st->runtime->cwidth_px * (st->text->curc - st->left));
  y = region->winy - lheight * l - 2;

  w = SUGG_LIST_WIDTH * st->runtime->cwidth_px + U.widget_unit;
  h = SUGG_LIST_SIZE * lheight + 0.4f * U.widget_unit;

  if (mval[0] < x || x + w < mval[0] || mval[1] < y - h || y < mval[1]) {
    return false;
  }

  /* Work out which of the items is at the top of the visible list. */
  for (i = 0, item = first; i < *top && item->next; i++, item = item->next) {
    /* Pass. */
  }

  /* Work out the target item index in the visible list. */
  tgti = (y - mval[1] - 4) / lheight;
  if (tgti < 0 || tgti > SUGG_LIST_SIZE) {
    return true;
  }

  for (i = tgti; i > 0 && item->next; i--, item = item->next) {
    /* Pass. */
  }
  if (item) {
    texttool_suggest_select(item);
  }
  return true;
}

void text_pop_suggest_list()
{
  SuggItem *item, *sel;
  int *top, i;

  item = texttool_suggest_first();
  sel = texttool_suggest_selected();
  top = texttool_suggest_top();

  i = 0;
  while (item && item != sel) {
    item = item->next;
    i++;
  }
  if (i > *top + SUGG_LIST_SIZE - 1) {
    *top = i - SUGG_LIST_SIZE + 1;
  }
  else if (i < *top) {
    *top = i;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Private API
 * \{ */

static void text_autocomplete_free(bContext *C, wmOperator *op);

static GHash *text_autocomplete_build(Text *text)
{
  GHash *gh;
  int seek_len = 0;
  const char *seek;
  texttool_text_clear();

  texttool_text_set_active(text);

  /* First get the word we're at. */
  {
    const int i = text_find_identifier_start(text->curl->line, text->curc);
    seek_len = text->curc - i;
    seek = text->curl->line + i;

    // BLI_strncpy_utf8(seek, seek_ptr, seek_len);
  }

  /* Now walk over entire doc and suggest words. */
  {
    gh = BLI_ghash_str_new(__func__);

    LISTBASE_FOREACH (TextLine *, linep, &text->lines) {
      size_t i_start = 0;
      size_t i_end = 0;
      size_t i_pos = 0;

      while (i_start < linep->len) {
        /* Seek identifier beginning. */
        i_pos = i_start;
        while ((i_start < linep->len) &&
               !text_check_identifier_nodigit_unicode(
                   BLI_str_utf8_as_unicode_step_safe(linep->line, linep->len, &i_pos)))
        {
          i_start = i_pos;
        }
        i_pos = i_end = i_start;
        while ((i_end < linep->len) &&
               text_check_identifier_unicode(
                   BLI_str_utf8_as_unicode_step_safe(linep->line, linep->len, &i_pos)))
        {
          i_end = i_pos;
        }

        if ((i_start != i_end) &&
            /* Check we're at the beginning of a line or that the previous char is not an
             * identifier this prevents digits from being added. */
            ((i_start < 1) || !text_check_identifier_unicode(
                                  BLI_str_utf8_as_unicode_or_error(&linep->line[i_start - 1]))))
        {
          char *str_sub = &linep->line[i_start];
          const int choice_len = i_end - i_start;

          if ((choice_len > seek_len) && (seek_len == 0 || STREQLEN(seek, str_sub, seek_len)) &&
              (seek != str_sub))
          {
            // printf("Adding: %s\n", s);
            char str_sub_last = str_sub[choice_len];
            str_sub[choice_len] = '\0';
            if (!BLI_ghash_lookup(gh, str_sub)) {
              char *str_dup = BLI_strdupn(str_sub, choice_len);
              /* A `set` would make more sense here. */
              BLI_ghash_insert(gh, str_dup, str_dup);
            }
            str_sub[choice_len] = str_sub_last;
          }
        }
        if (i_end != i_start) {
          i_start = i_end;
        }
        else {
          /* Highly unlikely, but prevent eternal loop. */
          i_start++;
        }
      }
    }

    {
      GHashIterator gh_iter;

      /* Get the formatter for highlighting. */
      TextFormatType *tft;
      tft = ED_text_format_get(text);

      GHASH_ITER (gh_iter, gh) {
        const char *s = static_cast<char *>(BLI_ghashIterator_getValue(&gh_iter));
        texttool_suggest_add(s, tft->format_identifier(s));
      }
    }
  }

  texttool_suggest_prefix(seek, seek_len);

  return gh;
}

/* -- */

static void get_suggest_prefix(Text *text, int offset)
{
  int i, len;
  const char *line;

  if (!text) {
    return;
  }
  if (!texttool_text_is_active(text)) {
    return;
  }

  line = text->curl->line;
  i = text_find_identifier_start(line, text->curc + offset);
  len = text->curc - i + offset;
  texttool_suggest_prefix(line + i, len);
}

static void confirm_suggestion(Text *text)
{
  SuggItem *sel;
  int i, over = 0;
  const char *line;

  if (!text) {
    return;
  }
  if (!texttool_text_is_active(text)) {
    return;
  }

  sel = texttool_suggest_selected();
  if (!sel) {
    return;
  }

  line = text->curl->line;
  i = text_find_identifier_start(line, text->curc /* - skipleft. */);
  over = text->curc - i;

  //  for (i = 0; i < skipleft; i++)
  //      txt_move_left(text, 0);
  BLI_assert(memcmp(sel->name, &line[i], over) == 0);
  const char *buf = sel->name + over;
  txt_insert_buf(text, buf, strlen(buf));

  //  for (i = 0; i < skipleft; i++)
  //      txt_move_right(text, 0);

  texttool_text_clear();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto Complete Operator
 * \{ */

static wmOperatorStatus text_autocomplete_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent * /*event*/)
{
  SpaceText *st = CTX_wm_space_text(C);
  Text *text = CTX_data_edit_text(C);

  st->doplugins = true;
  op->customdata = text_autocomplete_build(text);

  if (texttool_suggest_first()) {

    ED_area_tag_redraw(CTX_wm_area(C));

    if (texttool_suggest_first() == texttool_suggest_last()) {
      ED_text_undo_push_init(C);
      confirm_suggestion(st->text);
      text_update_line_edited(st->text->curl);
      text_autocomplete_free(C, op);
      ED_undo_push(C, op->type->name);
      return OPERATOR_FINISHED;
    }

    WM_event_add_modal_handler(C, op);
    return OPERATOR_RUNNING_MODAL;
  }
  text_autocomplete_free(C, op);
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus text_autocomplete_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* NOTE(@ideasman42): this code could be refactored or rewritten. */
  SpaceText *st = CTX_wm_space_text(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);

  int draw = 0, tools = 0, swallow = 0, scroll = 1;
  wmOperatorStatus retval = OPERATOR_RUNNING_MODAL;

  if (st->doplugins && texttool_text_is_active(st->text)) {
    if (texttool_suggest_first()) {
      tools |= TOOL_SUGG_LIST;
    }
  }

  switch (event->type) {
    case MOUSEMOVE: {
      if (space_text_do_suggest_select(st, region, event->mval)) {
        draw = 1;
      }
      swallow = 1;
      break;
    }
    case LEFTMOUSE:
      if (event->val == KM_PRESS) {
        if (space_text_do_suggest_select(st, region, event->mval)) {
          if (tools & TOOL_SUGG_LIST) {
            ED_text_undo_push_init(C);
            confirm_suggestion(st->text);
            text_update_line_edited(st->text->curl);
            ED_undo_push(C, op->type->name);
            swallow = 1;
            draw = 1;
          }
          retval = OPERATOR_FINISHED;
        }
        else {
          if (tools & TOOL_SUGG_LIST) {
            texttool_suggest_clear();
          }
          retval = OPERATOR_CANCELLED;
        }
        draw = 1;
      }
      break;
    case EVT_ESCKEY:
      if (event->val == KM_PRESS) {
        draw = swallow = 1;
        if (tools & TOOL_SUGG_LIST) {
          texttool_suggest_clear();
        }
        else {
          draw = swallow = 0;
        }
        retval = OPERATOR_CANCELLED;
      }
      break;
    case EVT_RETKEY:
    case EVT_PADENTER:
      if (event->val == KM_PRESS) {
        if (tools & TOOL_SUGG_LIST) {
          ED_text_undo_push_init(C);
          confirm_suggestion(st->text);
          text_update_line_edited(st->text->curl);
          ED_undo_push(C, op->type->name);
          swallow = 1;
          draw = 1;
        }
        retval = OPERATOR_FINISHED;
      }
      break;
    case EVT_LEFTARROWKEY:
    case EVT_BACKSPACEKEY:
      if (event->val == KM_PRESS) {
        if (tools & TOOL_SUGG_LIST) {
          if (event->modifier & KM_CTRL) {
            texttool_suggest_clear();
            retval = OPERATOR_CANCELLED;
            draw = 1;
          }
          else {
            /* Work out which char we are about to delete/pass. */
            if (st->text->curl && st->text->curc > 0) {
              char ch = st->text->curl->line[st->text->curc - 1];
              if ((ch == '_' || !ispunct(ch)) && !text_check_whitespace(ch)) {
                get_suggest_prefix(st->text, -1);
                text_pop_suggest_list();
                txt_move_left(st->text, false);
                draw = 1;
              }
              else {
                texttool_suggest_clear();
                retval = OPERATOR_CANCELLED;
                draw = 1;
              }
            }
            else {
              texttool_suggest_clear();
              retval = OPERATOR_CANCELLED;
              draw = 1;
            }
          }
        }
      }
      break;
    case EVT_RIGHTARROWKEY:
      if (event->val == KM_PRESS) {
        if (tools & TOOL_SUGG_LIST) {
          if (event->modifier & KM_CTRL) {
            texttool_suggest_clear();
            retval = OPERATOR_CANCELLED;
            draw = 1;
          }
          else {
            /* Work out which char we are about to pass. */
            if (st->text->curl && st->text->curc < st->text->curl->len) {
              char ch = st->text->curl->line[st->text->curc];
              if ((ch == '_' || !ispunct(ch)) && !text_check_whitespace(ch)) {
                get_suggest_prefix(st->text, 1);
                text_pop_suggest_list();
                txt_move_right(st->text, false);
                draw = 1;
              }
              else {
                texttool_suggest_clear();
                retval = OPERATOR_CANCELLED;
                draw = 1;
              }
            }
            else {
              texttool_suggest_clear();
              retval = OPERATOR_CANCELLED;
              draw = 1;
            }
          }
        }
      }
      break;
    case EVT_PAGEDOWNKEY:
      scroll = SUGG_LIST_SIZE - 1;
      ATTR_FALLTHROUGH;
    case WHEELDOWNMOUSE:
    case EVT_DOWNARROWKEY:
      if (event->val == KM_PRESS) {
        if (tools & TOOL_SUGG_LIST) {
          SuggItem *sel = texttool_suggest_selected();
          if (!sel) {
            texttool_suggest_select(texttool_suggest_first());
          }
          else {
            while (sel && scroll--) {
              if (sel != texttool_suggest_last() && sel->next) {
                texttool_suggest_select(sel->next);
                sel = sel->next;
              }
              else {
                texttool_suggest_select(texttool_suggest_first());
                sel = texttool_suggest_first();
              }
            }
          }
          text_pop_suggest_list();
          swallow = 1;
          draw = 1;
        }
      }
      break;
    case EVT_PAGEUPKEY:
      scroll = SUGG_LIST_SIZE - 1;
      ATTR_FALLTHROUGH;
    case WHEELUPMOUSE:
    case EVT_UPARROWKEY:
      if (event->val == KM_PRESS) {
        if (tools & TOOL_SUGG_LIST) {
          SuggItem *sel = texttool_suggest_selected();
          while (sel && scroll--) {
            if (sel != texttool_suggest_first() && sel->prev) {
              texttool_suggest_select(sel->prev);
              sel = sel->prev;
            }
            else {
              texttool_suggest_select(texttool_suggest_last());
              sel = texttool_suggest_last();
            }
          }
          text_pop_suggest_list();
          swallow = 1;
          draw = 1;
        }
      }
      break;
    case EVT_RIGHTSHIFTKEY:
    case EVT_LEFTSHIFTKEY:
      break;
    default: {
#if 0
      if (tools & TOOL_SUGG_LIST) {
        texttool_suggest_clear();
        draw = 1;
      }
#endif
      break;
    }
  }

  if (draw) {
    ED_area_tag_redraw(area);
  }

  if (swallow) {
    // retval = OPERATOR_RUNNING_MODAL;
  }

  if (texttool_suggest_first()) {
    if (retval != OPERATOR_RUNNING_MODAL) {
      text_autocomplete_free(C, op);
    }
    return retval;
  }
  text_autocomplete_free(C, op);
  return OPERATOR_FINISHED;
}

static void text_autocomplete_free(bContext *C, wmOperator *op)
{
  GHash *gh = static_cast<GHash *>(op->customdata);
  if (gh) {
    BLI_ghash_free(gh, nullptr, MEM_freeN);
    op->customdata = nullptr;
  }

  /* Other stuff. */
  {
    SpaceText *st = CTX_wm_space_text(C);
    st->doplugins = false;
    texttool_text_clear();
  }
}

static void text_autocomplete_cancel(bContext *C, wmOperator *op)
{
  text_autocomplete_free(C, op);
}

void TEXT_OT_autocomplete(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Text Auto Complete";
  ot->description = "Show a list of used text in the open document";
  ot->idname = "TEXT_OT_autocomplete";

  /* API callbacks. */
  ot->invoke = text_autocomplete_invoke;
  ot->cancel = text_autocomplete_cancel;
  ot->modal = text_autocomplete_modal;
  ot->poll = text_space_edit_poll;

  /* Flags. */
  /* Undo is handled conditionally by this operator. */
  ot->flag = OPTYPE_BLOCKING;
}

/** \} */
