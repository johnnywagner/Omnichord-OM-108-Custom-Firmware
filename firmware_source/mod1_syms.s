@ The stock addresses mod1.s branches to (Thumb bit set), as link-time symbols (fw/build.sh links them with mod1.o).
    .global stock_post, stock_b242, stock_b262
    .set    stock_post, FL(6000DB28) | 1     @ the MIDI queue post
    .set    stock_b242, FL(6000B242) | 1     @ the power-on preset store
    .set    stock_b262, FL(6000B262) | 1     @ the stock preset path after the replaced instructions
