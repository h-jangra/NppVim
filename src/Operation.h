#pragma once

void deleteToEndOfLine(HWND hwndEdit, int count);
void joinLines(HWND hwndEdit, int count);
void changeToEndOfLine(HWND hwndEdit, int count);
void applyOperatorToMotion(HWND hwndEdit, char op, char motion, int count);
std::pair<int, int> findWordBounds(HWND hwndEdit, int pos);
void deleteLineOnce(HWND hwndEdit);
void yankLineOnce(HWND hwndEdit);
void repeatLastOp(HWND hwndEdit);

void performSearch(HWND hwndEdit, const std::string& searchTerm, bool useRegex = false);
void searchNext(HWND hwndEdit);
void searchPrevious(HWND hwndEdit);
void showCurrentMatchPosition(HWND hwndEdit);
void updateSearchHighlight(HWND hwndEdit, const std::string& searchTerm, bool useRegex);
void clearSearchHighlights(HWND hwndEdit);