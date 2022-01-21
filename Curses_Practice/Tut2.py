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
    
    # Window creation :
    # Window : create smaller windows to control small portions
    # pad : show a certain amount of text at a time
    # place window on screen(terminal for this ex) and work in that
    # curses.newwin(height, char long, row, col)
    counter_win = curses.newwin(1, 20, 0, 18)
    
    # static string that isnt bothered by the window
    stdscr.addstr("Device input : ", BLUE_AND_YELLOW)
    stdscr.refresh()
    
    # changing color loop
    for i in range(100):
        # stdscr.clear()                      # needed bc curser wont reset to home r,c
        counter_win.clear()                   # windowing needs clear bc the window position is static 
        color = RED_AND_WHITE

        if i % 2 == 0:
            color = BLACK_AND_GREEN

        # stdscr.addstr(f"Counter {i} ", color)
        counter_win.addstr(f"Counter {i} ", color)
        # stdscr.refresh()
        counter_win.refresh()
        time.sleep(0.1)                       #delay by 0.2 sec

    # screen waits and gets char typed by user
    stdscr.getch()                      

wrapper(main)
