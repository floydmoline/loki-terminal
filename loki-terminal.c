// Loki Terminal Emulator
// Compile: gcc -o loki-terminal loki-terminal.c `pkg-config --cflags --libs gtk4 vte-2.91-gtk4`

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void on_child_exited(VteTerminal *terminal, gint status, gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    gtk_window_close(window);
}

static void copy_text(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    vte_terminal_copy_clipboard_format(terminal, VTE_FORMAT_TEXT);
}

static void paste_text(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    VteTerminal *terminal = VTE_TERMINAL(user_data);
    vte_terminal_paste_clipboard(terminal);
}

static void setup_terminal_colors(VteTerminal *terminal) {
    // Solarized Dark theme
    GdkRGBA fg, bg;
    gdk_rgba_parse(&fg, "#839496");
    gdk_rgba_parse(&bg, "#002b36");
    
    GdkRGBA palette[16];
    const char *palette_colors[] = {
        "#073642", "#dc322f", "#859900", "#b58900",
        "#268bd2", "#d33682", "#2aa198", "#eee8d5",
        "#002b36", "#cb4b16", "#586e75", "#657b83",
        "#839496", "#6c71c4", "#93a1a1", "#fdf6e3"
    };
    
    for (int i = 0; i < 16; i++) {
        gdk_rgba_parse(&palette[i], palette_colors[i]);
    }
    
    vte_terminal_set_colors(terminal, &fg, &bg, palette, 16);
}

typedef struct {
    GtkWindow *window;
    gchar **argv;
    gchar **envp;
} SpawnData;

static void spawn_callback(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data) {
    SpawnData *data = (SpawnData *)user_data;

    if (error) {
        fprintf(stderr, "Failed to spawn shell: %s\n", error->message);
        // Note: Do not call g_error_free(error) here - VTE owns this GError
        gtk_window_close(data->window);
    }

    // Free the spawn data now that the async operation is complete
    g_strfreev(data->argv);
    g_strfreev(data->envp);
    g_free(data);
}

static void spawn_shell(VteTerminal *terminal, GtkWindow *window) {
    const char *shell = g_getenv("SHELL");
    if (!shell) {
        shell = "/bin/bash";
    }

    SpawnData *data = g_new0(SpawnData, 1);
    data->window = window;
    data->argv = g_malloc0(sizeof(gchar*) * 2);
    data->argv[0] = g_strdup(shell);
    data->argv[1] = NULL;
    data->envp = g_get_environ();

    vte_terminal_spawn_async(
        terminal,
        VTE_PTY_DEFAULT,
        g_get_home_dir(),
        data->argv,
        data->envp,
        G_SPAWN_SEARCH_PATH,
        NULL,
        NULL,
        NULL,
        -1,
        NULL,
        spawn_callback,
        data
    );
}

static void activate(GtkApplication *app, gpointer user_data) {
    // Create window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Loki Terminal");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    
    // Create main box
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), main_box);
    
    // Create scrolled window
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(main_box), scrolled);
    
    // Create VTE terminal
    GtkWidget *terminal = vte_terminal_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), terminal);
    
    // Configure terminal
    PangoFontDescription *font = pango_font_description_from_string("Monospace 12");
    vte_terminal_set_font(VTE_TERMINAL(terminal), font);
    pango_font_description_free(font);
    
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), 10000);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(terminal), TRUE);
    
    // Set colors
    setup_terminal_colors(VTE_TERMINAL(terminal));
    
    // Connect child-exited signal
    g_signal_connect(terminal, "child-exited", G_CALLBACK(on_child_exited), window);
    
    // Create actions for copy/paste
    GSimpleAction *copy_action = g_simple_action_new("copy", NULL);
    g_signal_connect(copy_action, "activate", G_CALLBACK(copy_text), terminal);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(copy_action));
    
    GSimpleAction *paste_action = g_simple_action_new("paste", NULL);
    g_signal_connect(paste_action, "activate", G_CALLBACK(paste_text), terminal);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(paste_action));
    
    // Set up keyboard shortcuts
    const char *copy_accels[] = {"<Control><Shift>c", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.copy", copy_accels);
    
    const char *paste_accels[] = {"<Control><Shift>v", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "win.paste", paste_accels);
    
    // Show window first
    gtk_window_present(GTK_WINDOW(window));
    
    // Spawn shell after window is shown
    spawn_shell(VTE_TERMINAL(terminal), GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    // Disable GL to avoid GL context errors
    g_setenv("GDK_RENDERING", "cairo", TRUE);
    
    GtkApplication *app = gtk_application_new("com.example.loki-terminal", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
