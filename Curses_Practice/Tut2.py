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
    
    # clear screen
    #    stdscr.clear()

    # displays test as would print() in the terminal but cant use print bc curses overlay
    # (row, column, string, attributes)
    # text can be overwritten if row and col collide
    # text can also be formatted ex) curses.A_UNDERLINE, curses.A_BOLD
    #    stdscr.addstr(7, 7, "Hello World", curses.A_STANDOUT )

    # using color_pair as defined near the top
    #    stdscr.addstr(15, 7, "Hey Yall", curses.color_pair(1))
    #    stdscr.addstr(16, 7, "Hey Yall", BLUE_AND_YELLOW | curses.A_REVERSE)

    # refresh screen overlay 
    #    stdscr.refresh()


    # changing color loop
    for i in range(300):
        # stdscr.clear()                      # needed bc curser wont reset to home r,c
        color = RED_AND_WHITE

        if i % 2 == 0:
            color = BLACK_AND_GREEN

        stdscr.addstr(f"Counter {i} ", color)
        stdscr.refresh()
        time.sleep(0.1)                       #delay by 0.2 sec

    # screen waits and gets char typed by user
    stdscr.getch()                      

wrapper(main)
