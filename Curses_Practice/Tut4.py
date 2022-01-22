import curses
from curses import wrapper # init and pass obj directly to function
from curses.textpad import Textbox, rectangle
import time


def main(stdscr):  # stdscr is standard output screen( draw screen, overlay screen, then refresh screen)

    # create color pair (backgrounds and forground)
    # (ID #, foreground, background)
    curses.init_pair(1, curses.COLOR_BLUE, curses.COLOR_YELLOW)
    curses.init_pair(2, curses.COLOR_RED, curses.COLOR_WHITE)
    curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_GREEN)
    
    # Constant variables for color
    BLUE_AND_YELLOW = curses.color_pair(1)
    RED_AND_WHITE = curses.color_pair(2)
    BLACK_AND_GREEN = curses.color_pair(3)
    
   
    '''
    # screen waits and stores key
    key =stdscr.getkey()
    stdscr.addstr(5, 5 , f"Key: {key}")
    stdscr.refresh()
    stdscr.getch()

    # waiting on user input
    x, y = 0, 0
    string_x = 0
    while True:
        key = stdscr.getkey()
        if key == "KEY_LEFT":
            x -= 1
        elif key == "KEY_RIGHT":
            x += 1
        elif key == "KEY_UP":
            y -= 1
        elif key == "KEY_DOWN":
            y += 1

        stdscr.clear()
        string_x += 1
        stdscr.addstr(0, string_x//50, "Hello World!")
        stdscr.addstr(y, x, "0")
        stdscr.refresh()
    '''
    
    '''
    

    # nodelay allows the program to continue with warnings
    stdscr.nodelay(True)

    x, y = 0, 0
    string_x = 0
    
    while True:
        try:
            key = stdscr.getkey()
        except:
            key = None
        
        if key == "KEY_LEFT":
            x -= 1
        elif key == "KEY_RIGHT":
            x += 1
        elif key == "KEY_UP":
            y -= 1
        elif key == "KEY_DOWN":
            y += 1

        stdscr.clear()
        string_x += 1
        stdscr.addstr(0, string_x // 500, "Hello World!")
        stdscr.addstr(y, x, "0")
        stdscr.refresh()
    '''


    # draw rectangle at (start row, start col, width, height)
    rectangle(stdscr, 2, 2, 20, 20)

    
    # creating window(width, height, start row, start col) for textbox
    win = curses.newwin(17, 17, 3, 3)
    box = Textbox(win)              # gives emac like text commands inside the box ctrl+G gets outside the box
    stdscr.refresh()

    # takes user input to edit the box (ctrl+g to get out)
    box.edit()

    # gather text that user input that removes newline(.replace) and remove trailing white spaces (strip)
    text = box.gather().strip().replace("\n", "")

    # display the text
    stdscr.addstr(10, 40, text)
    
    
    stdscr.getch()

    
    

wrapper(main)
