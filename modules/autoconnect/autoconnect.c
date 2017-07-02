/**
 * @file autoconnect.c Automatic Connection module
 *
 * Copyright (C) 2010 Creytiv.com
 * Copyright (C) 2017 Jonathan Sieber
 */
#include <re.h>
#include <baresip.h>

/**
 * @defgroup autoconnect autoconnect
 *
 * Always keep one connection open to any available contact
 *
 * NOTE: This module is experimental.
 *
 */
 
 /* Potentially Useful Stuff */
 
 /* Check active calls */
static bool have_active_calls(void)
{
	struct le *le;

	for (le = list_head(uag_list()); le; le = le->next) {

		struct ua *ua = le->data;

		if (ua_call(ua))
			return true;
	}

	return false;
}


struct tmr auto_timer;

static int make_call(const char* uri)
{
	debug("autoconnect: calling %s\n", uri);
	int err;
	struct call *call;
	
	err = ua_connect(uag_current(), &call, NULL, uri, NULL, VIDMODE_ON);
	return err;
}

static void check_state(void* arg)
{
	debug("autoconnect: check_state timer occured\n");
	tmr_start(&auto_timer, 1000, check_state, 0);
	
	if (!uag_current()) {
		error("UA not available (yet)\n");
		return;
	}
	
	if (have_active_calls()) {
		debug("autoconnect: Still in call, doing nothing\n");
		tmr_start(&auto_timer, 30000, check_state, 0);
		return;
	}
	// TODO: if connected, do nothing, or un-set timer?
	// Enumerate Contacts
	int err = 0;
	struct le *le;
	struct contacts *contacts = baresip_contacts();
	for (le = list_head(contact_list(contacts)); le; le = le->next) {
		struct contact *c = le->data;
		
		if (contact_presence(c) == PRESENCE_OPEN) {
			err = make_call(contact_str(c));
		}
		
		if (err) {
			error("autoconnect: Error in making call to %r: %m\n",
				&contact_addr(c)->uri.host, err);
			return;
		}
	}
	
	error("No contacts currently available\n");
}

static void ua_event_handler(struct ua *ua, enum ua_event ev,
			  struct call *call, const char *prm, void *arg)
{
	error("autoconnect: UA Event: %s %s\n", uag_event_str(ev), prm);

	switch (ev) {
		case UA_EVENT_CALL_ESTABLISHED:
			ua_presence_status_set(ua, PRESENCE_BUSY);
			break;
			
		case UA_EVENT_CALL_CLOSED:
			ua_presence_status_set(ua, PRESENCE_OPEN);
			break;
		default:
			break;
	}
}


static int module_init(void)
{
	int err;
	
	debug("autoconnect: module_init\n");
	
	tmr_init(&auto_timer);
	tmr_start(&auto_timer, 1337, check_state, 0);

	uag_event_register(ua_event_handler, 0);

	err = 0;
	if (err)
		return err;

	return 0;
}


static int module_close(void)
{
	debug("autoconnect: module_close\n");

	uag_event_unregister(ua_event_handler);

	return 0;
}


const struct mod_export DECL_EXPORTS(autoconnect) = {
	"autoconnect",
	"application",
	module_init,
	module_close
};
