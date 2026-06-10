/*
 * Copyright LWJGL. All rights reserved.
 * License terms: https://www.lwjgl.org/license
 */
package org.lwjgl.util.nfd;

import org.jspecify.annotations.*;

/**
 * Android bridge for NFD (Native File Dialog).
 *
 * <p>On Android, the NFD native code delegates to this class via JNI. The app must register
 * a {@link FileDialogProvider} implementation via {@link #setProvider(FileDialogProvider)}
 * before calling any NFD functions.</p>
 *
 * <p>This class contains no Android SDK dependencies and can be compiled with a standard JDK.
 * The actual file dialog implementation (using Android's Storage Access Framework) is provided
 * at runtime by the Android application (e.g. PojavLauncher).</p>
 */
public final class NFDAndroid {

    private NFDAndroid() {
    }

    /**
     * Interface for implementing Android file dialogs.
     *
     * <p>The methods return the selected file path(s) as strings, or {@code null} if the user
     * cancelled the dialog. The {@code filterSpecs} parameter is a flat array of
     * {@code [name, spec, name, spec, ...]} pairs (or {@code null} if no filters).</p>
     */
    public interface FileDialogProvider {

        /**
         * Open a single file dialog.
         *
         * @param filterSpecs flat array of [name, spec, name, spec, ...] pairs, or {@code null}
         * @param defaultPath initial directory path, or {@code null}
         * @param defaultName default file name, or {@code null}
         * @return selected file path, or {@code null} if cancelled
         */
        @Nullable String openDialog(@Nullable String[] filterSpecs, @Nullable String defaultPath, @Nullable String defaultName);

        /**
         * Open a file dialog for multiple file selection.
         *
         * @param filterSpecs flat array of [name, spec, name, spec, ...] pairs, or {@code null}
         * @param defaultPath initial directory path, or {@code null}
         * @return array of selected file paths, or {@code null} if cancelled
         */
        @Nullable String[] openDialogMultiple(@Nullable String[] filterSpecs, @Nullable String defaultPath);

        /**
         * Open a save file dialog.
         *
         * @param filterSpecs flat array of [name, spec, name, spec, ...] pairs, or {@code null}
         * @param defaultPath initial directory path, or {@code null}
         * @param defaultName default file name, or {@code null}
         * @return selected file path, or {@code null} if cancelled
         */
        @Nullable String saveDialog(@Nullable String[] filterSpecs, @Nullable String defaultPath, @Nullable String defaultName);

        /**
         * Pick a single folder.
         *
         * @param defaultPath initial directory path, or {@code null}
         * @return selected folder path, or {@code null} if cancelled
         */
        @Nullable String pickFolder(@Nullable String defaultPath);

        /**
         * Pick multiple folders.
         *
         * @param defaultPath initial directory path, or {@code null}
         * @return array of selected folder paths, or {@code null} if cancelled
         */
        @Nullable String[] pickFolderMultiple(@Nullable String defaultPath);
    }

    private static volatile @Nullable FileDialogProvider provider;

    /**
     * Registers the file dialog provider.
     *
     * <p>Must be called before any NFD functions are invoked. The provider typically
     * implements Android's Storage Access Framework via Intents.</p>
     *
     * @param provider the file dialog provider implementation
     */
    public static void setProvider(@Nullable FileDialogProvider provider) {
        NFDAndroid.provider = provider;
    }

    /**
     * Returns the registered file dialog provider, or {@code null}.
     */
    @Nullable
    public static FileDialogProvider getProvider() {
        return provider;
    }

    // ---- JNI bridge methods (called from nfd_android.cpp) ----

    @Nullable
    static String openDialog(@Nullable String[] filterSpecs, @Nullable String defaultPath, @Nullable String defaultName) {
        FileDialogProvider p = provider;
        if (p == null) {
            return null;
        }
        return p.openDialog(filterSpecs, defaultPath, defaultName);
    }

    @Nullable
    static String[] openDialogMultiple(@Nullable String[] filterSpecs, @Nullable String defaultPath) {
        FileDialogProvider p = provider;
        if (p == null) {
            return null;
        }
        return p.openDialogMultiple(filterSpecs, defaultPath);
    }

    @Nullable
    static String saveDialog(@Nullable String[] filterSpecs, @Nullable String defaultPath, @Nullable String defaultName) {
        FileDialogProvider p = provider;
        if (p == null) {
            return null;
        }
        return p.saveDialog(filterSpecs, defaultPath, defaultName);
    }

    @Nullable
    static String pickFolder(@Nullable String defaultPath, @Nullable String ignored) {
        FileDialogProvider p = provider;
        if (p == null) {
            return null;
        }
        return p.pickFolder(defaultPath);
    }

    @Nullable
    static String[] pickFolderMultiple(@Nullable String[] filterSpecs, @Nullable String defaultPath) {
        FileDialogProvider p = provider;
        if (p == null) {
            return null;
        }
        return p.pickFolderMultiple(defaultPath);
    }
}
