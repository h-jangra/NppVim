#pragma once

void DoMotion_char_left(HWND hwndEdit, int count);
void DoMotion_char_right(HWND hwndEdit, int count);
void DoMotion_line_up(HWND hwndEdit, int count);
void DoMotion_end_word_prev(HWND hwndEdit, int count);
void DoMotion_next_char(HWND hwndEdit, int count, char searchChar);
void DoMotion_prev_char(HWND hwndEdit, int count, char searchChar);
void DoMotion_line_down(HWND hwndEdit, int count);
void DoMotion_word_right(HWND hwndEdit, int count);
void DoMotion_word_left(HWND hwndEdit, int count);
void DoMotion_end_word(HWND hwndEdit, int count);
void DoMotion_end_line(HWND hwndEdit, int count);
void DoMotion_line_start(HWND hwndEdit, int count);
void doMotion(HWND hwndEdit, char motion, int count);

void Tab_next(HWND hwndEdit, int count);
void Tab_previous(HWND hwndEdit, int count);
