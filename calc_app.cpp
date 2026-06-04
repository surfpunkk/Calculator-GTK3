#include "calc_app.h"

Calculator::Calculator() : header_bar(nullptr), undo_button(nullptr), theme_switch(nullptr), history_button(nullptr),
window(nullptr), grid(nullptr), entry(nullptr) {}

void Calculator::load_css(const char* css_path) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    static GtkCssProvider *old_provider = nullptr;
    if (old_provider) {
        gtk_style_context_remove_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(old_provider)
        );
        g_object_unref(old_provider);
    }
    old_provider = provider;
    gtk_css_provider_load_from_resource(provider, css_path);

    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

void Calculator::setup_header_bar() {
    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Calculator");
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    undo_button = gtk_button_new_with_label("↩");
    g_signal_connect(undo_button, "clicked", G_CALLBACK(+[](GtkButton*, const Calculator* self) {
        self->undo_last_action();
    }), this);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), undo_button);

    history_button = gtk_button_new_with_label("⧖");
    g_signal_connect(history_button, "clicked", G_CALLBACK(+[](GtkButton*, Calculator* self) {
        self->show_history();
    }), this);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar),history_button);

    theme_switch = gtk_switch_new();
    g_signal_connect(theme_switch, "state-set", G_CALLBACK(+[](GtkSwitch*, const gboolean state, Calculator* self) {
        self->dark_theme = state;
        self->toggle_theme();
        return FALSE;
    }), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), theme_switch);
}

void Calculator::undo_last_action() const {
    if (!history_stack.empty()) {
        const std::string prev_expression = history_stack.top();
        history_stack.pop();
        gtk_entry_set_text(GTK_ENTRY(entry), prev_expression.c_str());
    }
}

void Calculator::show_history() {
    GtkWidget* menu = gtk_menu_new();
    if (history.empty()) {
        GtkWidget* menu_item = gtk_menu_item_new_with_label("History is empty");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
        gtk_widget_set_sensitive(menu_item, FALSE);
    } else {
        for (const auto& item : history) {
            GtkWidget* menu_item = gtk_menu_item_new_with_label(item.c_str());
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
            g_signal_connect(menu_item, "activate",
                             G_CALLBACK(+[](GtkMenuItem* item, gpointer data) {
                                 auto self = static_cast<Calculator*>(data);
                                 const gchar* label = gtk_menu_item_get_label(item);
                                 std::string str_label = label;
                                 str_label.erase((str_label.find('=') - 1));
                                 gtk_entry_set_text(GTK_ENTRY(self->entry), str_label.c_str());
                             }),
                             this);
        }
    }
    gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_widget(GTK_MENU(menu),GTK_WIDGET(history_button),GDK_GRAVITY_SOUTH_WEST,
                             GDK_GRAVITY_NORTH_WEST,
                             nullptr);
}

bool Calculator::is_system_dark_theme() {
    GtkSettings* settings = gtk_settings_get_default();
    gchar* theme_name = nullptr;
    g_object_get(settings, "gtk-theme-name", &theme_name, nullptr);

    const bool is_dark = theme_name && (
        g_strrstr(theme_name, "dark") ||
        g_strrstr(theme_name, "Dark") ||
        g_strrstr(theme_name, "DARK")
    );
    g_free(theme_name);
    return is_dark;
}

void Calculator::toggle_theme() const {
    GdkScreen *screen = gdk_screen_get_default();
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_style_context_remove_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(provider)
    );
    g_object_unref(provider);
    if (dark_theme) {
        load_css("/org/calc/styles/dark_styles.css");
    } else {
        load_css("/org/calc/styles/styles.css");
    }
    gtk_widget_queue_draw(window);
}

void Calculator::setup_ui() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 400);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_deletable(GTK_WINDOW(window), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(window), TRUE);
    gtk_window_set_modal(GTK_WINDOW(window), FALSE);
    if (GdkPixbuf *pixbuf = gdk_pixbuf_new_from_resource("/org/calc/styles/icon.png", nullptr)) {
        gtk_window_set_icon(GTK_WINDOW(window), pixbuf);
        g_object_unref(pixbuf);
    }
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    setup_header_bar();
    dark_theme = is_system_dark_theme();
    gtk_switch_set_active(GTK_SWITCH(theme_switch), dark_theme);
    toggle_theme();

    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), grid);

    entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
    gtk_widget_set_size_request(entry, 400, 200);
    gtk_widget_set_name(entry, "entry-field");
    gtk_grid_attach(GTK_GRID(grid), entry, 0, 0, 5, 1);

    g_signal_connect(entry, "key-press-event", G_CALLBACK(on_key_press), nullptr);

    const char *buttons[] = {
        "(", ")", "𝑥ⁿ", "C", "⌫",
        "7", "8", "9", "÷", "%",
        "4", "5", "6", "×", "√",
        "1", "2", "3", "-", "𝑥!",
        "0", ",", "=", "+", "π"
    };
    for (int i = 0; i < 25; ++i) {
        GtkWidget *button = gtk_button_new_with_label(buttons[i]);
        gtk_widget_set_size_request(button, 50, 50);
        if (buttons[i] == "=") gtk_widget_set_name(button, "button_equal");
        else if (buttons [i] == "⌫") gtk_widget_set_name(button, "button_del");
        else if (buttons [i] == "𝑥ⁿ") gtk_widget_set_name(button, "xn");
        else gtk_widget_set_name(button, "calc_buttons");
        gtk_grid_attach(GTK_GRID(grid), button, i % 5, i / 5 + 1, 1, 1);
        g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), entry);
    }
    gtk_widget_show_all(window);
}

int Calculator::run(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    setup_ui();
    gtk_main();
    return 0;
}

void Calculator::on_button_clicked(GtkWidget *widget, gpointer data) {
    const char *label = gtk_button_get_label(GTK_BUTTON(widget));
    GtkWidget *entry = GTK_WIDGET(data);
    EventHandlers::handle_input(entry, label);
}

gboolean Calculator::on_key_press(GtkWidget *widget, const GdkEventKey *event) {
    const char *key = nullptr;
    gint start_pos = 0, end_pos = 0;
    switch (event->keyval) {
        case GDK_KEY_Return: case GDK_KEY_KP_Enter: case GDK_KEY_equal: key = "=";
            break;
        case GDK_KEY_0: case GDK_KEY_1: case GDK_KEY_2: case GDK_KEY_3: case GDK_KEY_4:
        case GDK_KEY_5: case GDK_KEY_6: case GDK_KEY_7: case GDK_KEY_8: case GDK_KEY_9:
            key = gdk_keyval_name(event->keyval);
            break;
        case GDK_KEY_KP_0 ... GDK_KEY_KP_9: key = gdk_keyval_name(event->keyval - GDK_KEY_KP_0 + GDK_KEY_0);
            break;
        case GDK_KEY_period: case GDK_KEY_comma: case GDK_KEY_KP_Decimal: key = ",";
            break;
        case GDK_KEY_asterisk: case GDK_KEY_KP_Multiply: case GDK_KEY_8 & GDK_SHIFT_MASK: key = "×";
            break;
        case GDK_KEY_slash: case GDK_KEY_KP_Divide: key = "÷";
            break;
        case GDK_KEY_plus: case GDK_KEY_KP_Add: key = "+";
            break;
        case GDK_KEY_minus: case GDK_KEY_KP_Subtract: key = "-";
            break;
        case GDK_KEY_Escape: case GDK_KEY_Delete: key = "C";
            break;
        case GDK_KEY_BackSpace:
            EventHandlers::handle_backspace(GTK_ENTRY(widget));
            return TRUE;
        case GDK_KEY_parenleft: key = "(";
            break;
        case GDK_KEY_parenright: key = ")";
            break;
        case GDK_KEY_asciicircum: case GDK_KEY_dead_circumflex: key = "𝑥ⁿ";
            break;
        case GDK_KEY_exclam: key = "𝑥!";
            break;
        case GDK_KEY_percent: key = "%";
            break;
        case GDK_KEY_p: case GDK_KEY_P: key = "π";
            break;
        default:
            const gchar *current_text = gtk_entry_get_text(GTK_ENTRY(widget));
            if (g_strrstr(current_text, "Error:") != nullptr) return TRUE;
            return FALSE;
    }
    if (key != nullptr) {
        if (gtk_editable_get_selection_bounds(GTK_EDITABLE(widget), &start_pos, &end_pos)) {
            gtk_editable_delete_selection(GTK_EDITABLE(widget));
        }
        EventHandlers::handle_input(widget, key);
        gtk_editable_set_position(GTK_EDITABLE(widget), -1);
        return TRUE;
    }
    return FALSE;
}