
#define LCD_PAGE_MAX  3             //max count LCD pages
#define LCD_PAGE_VISIBLE  5000      // switch to next page after xx ms

//Function prototypes
void setup_lcd(); 
void lcd_error(int code);
void display_page();
void lcd_page1();
