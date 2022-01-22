import curses
from curses import wrapper # init and pass obj directly to function
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
    
    
    # pad : show a certain amount of text at a time, overflow is possible
    # can show only portions of the pad and trim the other
    # place window on screen(terminal for this ex) and work in that
    #     curse.newpad(curser row, curser col, length, width)
    
    pad = curses.newpad(100, 100)
    stdscr.refresh()
    
    for i in range(100):
        for j in range(26):
            char = chr(65+j)
            pad.addstr(char, BLACK_AND_GREEN) # adds to curser bc row/col isnt specified

            
    # (start pad row, start pad col, start window row, start window col, end window row, end window col) 
    # pad.refresh(0,0,5,5,25,7)

    # scrolling pads
    for i in range(50):
        stdscr.clear()
        stdscr.refresh()
        pad.refresh(0,0, 5, 5+i, 25, 25+i)
        time.sleep(0.2)
    
    # screen waits and gets char typed by user
    stdscr.getch()                      

wrapper(main)
