// Provides Node.JS binding for ldap_simple_bind().
// See README
// 2010, Joe Walnes, joe@walnes.com, http://joewalnes.com/


/*
Here's the basic flow of events. LibEIO is used to ensure that
the LDAP calls occur on a background thread and do not block
the main Node event loop.

+----------------------+                +------------------------+
| Main Node Event Loop |                | Background Thread Pool |
+----------------------+                +------------------------+

User application
|
V
JavaScript: authenticate()
|
V
ldapauth.cc: Authenticate()
|
+-------------------------> libauth.cc: EIO_Authenticate()
|                                      |
V                                      V
(user application carries               ldap_simple_bind()
on doing its stuff)                          |
|                              (wait for response
(no blocking)                           from server)
|                                      |
(sometime later)                         (got response)
|                                      |
ldapauth.cc: EIO_AfterAuthenticate() <----------+
|
V
Invoke user supplied JS callback

*/

#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <ldap.h>
#include <unistd.h>
#include <stdlib.h>

using namespace v8;
using namespace std;

#define THROW(message) ThrowException(Exception::TypeError(String::New(message)))

// Data passed between threads
struct auth_request 
{
// Input params
	char *uri;
	LDAPURLDesc *ludpp;
	LDAP *ldap;
// Callback function
	Persistent<Function> cbSuccess;
	Persistent<Function> cbFailure;
// Result
	bool connected;
	bool authenticated;
};

// Runs on background thread, performing the actual LDAP request.
static int EIO_Open(eio_req *req) 
{
	HandleScope scope;
	int	ver = 3;
	struct auth_request *auth_req = (struct auth_request*)(req->data);
	unsigned long int len = 5 + sizeof(auth_req->ludpp->lud_scheme) + sizeof(auth_req->ludpp->lud_host) + sizeof(auth_req->ludpp->lud_port);
	char uri[len];
	
	sprintf(uri, "%s://%s:%d/", auth_req->ludpp->lud_scheme, auth_req->ludpp->lud_host, auth_req->ludpp->lud_port);
	
	auth_req->ldap = ldap_init(auth_req->ludpp->lud_host, auth_req->ludpp->lud_port);
	
	int res = ldap_initialize(&auth_req->ldap, uri);
	
	if (auth_req->ldap == NULL || res < 0) {
		auth_req->connected = false;
	} else {
		//ldap_set_option(ldap, LDAP_OPT_RESTART, LDAP_OPT_ON);
		ldap_set_option(auth_req->ldap, LDAP_OPT_PROTOCOL_VERSION, &ver);
		
		auth_req->connected = true;
	}
	
	return 0;
}

// Called on main event loop when background thread has completed
static int EIO_AfterOpen(eio_req *req) 
{
	ev_unref(EV_DEFAULT_UC);
	HandleScope scope;
	Local<Array>	js_result_list;
	LDAPMessage *ldap_res;
	struct auth_request *auth_req = (struct auth_request *)(req->data);
	int res, res1, msgid;
	Handle<Value> callback_args[2];
	
	callback_args[0] = Local<Value>::New(Boolean::New(true));
	callback_args[1] = (Handle<Value>)Undefined();
	
	if (auth_req->connected == true) {
		res = ldap_search(auth_req->ldap, auth_req->ludpp->lud_dn, LDAP_SCOPE_SUBTREE, auth_req->ludpp->lud_filter, auth_req->ludpp->lud_attrs, 0);
		
		if (res != LDAP_SERVER_DOWN) {
			if ((res1 = ldap_result(auth_req->ldap, LDAP_RES_ANY, 1, NULL, &ldap_res)) >= 1) {
				
				msgid = ldap_msgid(ldap_res);

				switch(res1) {
					case	LDAP_RES_SEARCH_RESULT: {
						LDAPMessage * entry = NULL;
						BerElement * berptr = NULL;
						char * attrname			= NULL;
						char ** vals;
						Local<Object>	js_result;
						Local<Array>	js_attr_vals;
						int j;
						char * dn;

						int entry_count = ldap_count_entries(auth_req->ldap, ldap_res);
						js_result_list = Array::New(entry_count);

						for (entry = ldap_first_entry(auth_req->ldap, ldap_res), j = 0 ; entry ;
						entry = ldap_next_entry(auth_req->ldap, entry), j++) {
							js_result = Object::New();
							js_result_list->Set(Integer::New(j), js_result);

							dn = ldap_get_dn(auth_req->ldap, entry);

							for (attrname = ldap_first_attribute(auth_req->ldap, entry, &berptr) ;
							attrname ; attrname = ldap_next_attribute(auth_req->ldap, entry, berptr)) {
								vals = ldap_get_values(auth_req->ldap, entry, attrname);
								int num_vals = ldap_count_values(vals);
								js_attr_vals = Array::New(num_vals);
								js_result->Set(String::New(attrname), js_attr_vals);
								for (int i = 0 ; vals[i] ; i++) {
									js_attr_vals->Set(Integer::New(i), String::New(vals[i]));
								} // all values for this attr added.
								ldap_value_free(vals);
								ldap_memfree(attrname);
							} // attrs for this entry added. Next entry.
							js_result->Set(String::New("dn"), String::New(dn));
							ber_free(berptr,0);
							ldap_memfree(dn);
						} // all entries done.	
						
						callback_args[0] = (Handle<Value>)Undefined();
						callback_args[1] = js_result_list;
					} break;

					default: {
						callback_args[0] = Local<Value>::New(String::New(ldap_err2string(res1)));
						callback_args[1] = (Handle<Value>)Undefined();
					} break;
				}

				ldap_msgfree(ldap_res);
			} else {	
				callback_args[0] = Local<Value>::New(String::New(ldap_err2string(res1)));
				callback_args[1] = (Handle<Value>)Undefined();
			}
		}
	}
	
	// Invoke callback JS function
	auth_req->cbSuccess->Call(Context::GetCurrent()->Global(), 2, callback_args);
	
	// Cleanup auth_request struct
	
	auth_req->cbSuccess.Dispose();
	ldap_free_urldesc(auth_req->ludpp);
	free(auth_req);
	
	return 0;
}

// Exposed authenticate() JavaScript function
static Handle<Value> Open(const Arguments& args)
{
	HandleScope scope;
	
// Validate args.
	if (args.Length() < 2)      return THROW("Required arguments: ldap_uri, callback");
	if (!args[0]->IsString())   return THROW("ldap_uri should be a string");
	if (!args[1]->IsFunction()) return THROW("success callback should be a function");
	
	if (args.Length() == 3) {
		if (!args[2]->IsFunction()) {
			return THROW("error callback should be a function");
		}
	}
	
// Input params.
	String::Utf8Value uri(args[0]);
	Local<Function> cbSuccess = Local<Function>::Cast(args[1]);
	Local<Function> cbFailure;
	
	if (args.Length() == 3) {
		cbFailure = Local<Function>::Cast(args[2]);
	}
	
// Store all parameters in auth_request struct, which shall be passed across threads.
	struct auth_request *auth_req = (struct auth_request*) calloc(1, sizeof(struct auth_request));
	auth_req->uri = strdup(*uri);
	auth_req->cbSuccess = Persistent<Function>::New(cbSuccess);
	
	if (args.Length() == 3) {
		auth_req->cbFailure = Persistent<Function>::New(cbFailure);	
	}
	
	if (ldap_url_parse(auth_req->uri, &auth_req->ludpp) == 0) {
		eio_custom(EIO_Open, EIO_PRI_DEFAULT, EIO_AfterOpen, auth_req);
		ev_ref(EV_DEFAULT_UC);
	} else {
		ldap_free_urldesc(auth_req->ludpp);
		Handle<Value> callback_args[2];

		callback_args[0] = Local<Value>::New(String::New("Invalid LDAP URI"));
		callback_args[1] = (Handle<Value>)Undefined();
		
		if (args.Length() == 3) {
			auth_req->cbFailure->Call(Context::GetCurrent()->Global(), 2, callback_args);
			auth_req->cbFailure.Dispose();
		} else {
			auth_req->cbSuccess->Call(Context::GetCurrent()->Global(), 2, callback_args);
			auth_req->cbSuccess.Dispose();
		}
		
		free(auth_req);
	}
	
	
	return Undefined();
}

// Entry point for native Node module
extern "C" void
	init (Handle<Object> target) 
{
	HandleScope scope;
	target->Set(String::New("Search"), FunctionTemplate::New(Open)->GetFunction());
}