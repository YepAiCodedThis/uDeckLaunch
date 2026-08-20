#include "theme.h"

void dh_theme_init(DhTheme *t)
{
    t->bg = (SDL_Color){DH_COL_BG};
    t->panel = (SDL_Color){DH_COL_PANEL};
    t->header = (SDL_Color){DH_COL_HEADER};
    t->accent = (SDL_Color){DH_COL_ACCENT};
    t->text = (SDL_Color){DH_COL_TEXT};
    t->muted = (SDL_Color){DH_COL_MUTED};
    t->green = (SDL_Color){DH_COL_GREEN};
    t->card = (SDL_Color){DH_COL_CARD};
    t->white = (SDL_Color){DH_COL_WHITE};
    t->pill = (SDL_Color){DH_COL_PILL};
    t->row = (SDL_Color){DH_COL_ROW};
    t->row_on = (SDL_Color){DH_COL_ROW_ON};
    t->danger = (SDL_Color){DH_COL_DANGER};
}
