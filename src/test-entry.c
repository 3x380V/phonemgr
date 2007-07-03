
#include <gtk/gtk.h>
#include <string.h>
#include "e-phone-entry.h"

#define GCONF_COMPLETION "/apps/evolution/addressbook"
#define GCONF_COMPLETION_SOURCES GCONF_COMPLETION "/sources"

#define CONTACT_FORMAT "%s (%s)"

static char *phone_number = NULL;
static GtkWidget *activate;

static void
phone_changed_cb (EPhoneEntry *pentry, const char *number, gpointer data)
{
	g_free (phone_number);
	phone_number = g_strdup (number);

	g_message ("phone number: %s", number);

	gtk_widget_set_sensitive (activate, (number != NULL));
}

int main (int argc, char **argv)
{
	GtkWidget *window, *entry;

	g_thread_init (NULL);

	gtk_init (&argc, &argv);

	window = gtk_dialog_new ();
	activate = gtk_dialog_add_button (GTK_DIALOG (window), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_widget_set_sensitive (activate, FALSE);
	entry = e_phone_entry_new ();
	g_signal_connect (G_OBJECT (entry), "phone_changed",
			G_CALLBACK (phone_changed_cb), NULL);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (window)->vbox), entry);

	gtk_widget_show_all (window);

	gtk_dialog_run (GTK_DIALOG (window));
	g_print ("Phone number selected: %s\n", phone_number ? phone_number : "None");

	return 0;
}

