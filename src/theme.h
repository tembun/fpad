#define STR2(X) #X
#define STR(X) STR2(X)

#define QCOLOR(C) (QColor(STR(C)))

#define THEME_LIGHT 0
#define THEME_DARK 1

#define THEME THEME_LIGHT

#if THEME == THEME_LIGHT
	#define TEXT_BG #ffed94
	#define TEXT_FG #000000
	#define TEXT_SELECT_BG #cf7339
	#define TEXT_SELECT_FG #000000
	#define WIDGETS_BG #fff2b0
	#define WIDGETS_FG #000000
	#define TAB_BG #fff2b0
	#define TAB_FG #000000
	#define TAB_SELECTED_BG #e7ffff
	#define TAB_SELECTED_FG #000000
	#define TAB_BORDER #256969
	#define SCROLL_BG #fff2b0
	#define SCROLL_FG #9a9a4b
#endif

#if THEME == THEME_DARK
	#define TEXT_BG #fff4be
	#define TEXT_FG #000000
	#define TEXT_SELECT_BG #9e9e9e
	#define TEXT_SELECT_FG #000000
	#define WIDGETS_BG #303030
	#define WIDGETS_FG #ffffff
	#define SCROLL_BG #303030
	#define SCROLL_FG #0b0b0b
#endif
