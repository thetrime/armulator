# armulator

ARM simulator. Currently just a toy, but it can:
   * Load Mach-O executables
      * including dylibs (mostly - no code relocation yet)
   * Decode quite a few instructions, including Thumb and Thumb2
   * Execute all the instructions it can decode

Some things that are planned:
   * More faithful implementation of the CSPR register
   * Perhaps a better implementation of the instruction pipeline so that we don't need the next_instruction hack
   * Decode and execute a lot more instructions
  
