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


    # echo the users keystrokes
    curses.echo()
    
    # add border around the screen
    stdscr.attron(BLACK_AND_GREEN)
    stdscr.border()
    stdscr.attroff(BLACK_AND_GREEN)
    
    # Toggle On attributes
    stdscr.attron(BLUE_AND_YELLOW)

    # draw rectangle at (start row, start col, width, height)
    rectangle(stdscr, 1, 1, 5, 20)

    # Toggle OFF attributes
    stdscr.attroff(BLUE_AND_YELLOW)

    # str outside the attributes
    stdscr.addstr(5, 30, "Hello People!")


    # move the curser
    stdscr.move(10, 20)

    while True:
        key = stdscr.getkey()
        if key == "q":
            break
    
    
    stdscr.refresh()
    stdscr.getch()

    
    

wrapper(main)
