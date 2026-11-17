package org.oxygen32x;

import org.libsdl.app.SDLActivity;

/** The SDL activity: runs libmain.so (the oxygen32x player). */
public class GameActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
