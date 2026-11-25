## FormKeyDown, the keys Windows sent the form, as virtual key codes. Only
## those of the screens ported so far are handled.

import ../../delphix/[pascal, delphix]
import chavesvar

proc FormKeyDown*(key: Word) =
  if key == ord('5') and creditos < 99:
    inc creditos
    if DXWaveListSnd.Count > 0: DXWaveListSnd.Play(0)
    if tempodemo > 0 or tela == 8:
      if creditos > 0:
        tela := 1
        contador := 500
        piscar := 0
        cameray := -200
        tempodemo := 0
