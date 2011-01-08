LDAP Search Bindings for Node.JS
================================

This is a fork and merge of [node-LDAP](https://github.com/jeremycx/node-LDAP) and [node-ldapauth](https://github.com/joewalnes/node-ldapauth).

Provides a simple LDAP bindings function for running search queries on an LDAP server.

It has SSL (ldaps) and LDAP URI support see `man 3 ldap_url`.

It does not yet support LDAP bind.

It is still experimental, YMMV!

Building
--------

	node-waf configure build

Usage
-----

Ensure libldap (OpenLDAP client library) is installed.

You need to add ldap.node to your application.

	var LDAPClient	= require("./build/default/ldap.node"); // Path to ldapauth.node
	var LDAPUri		= "ldaps://ldap.example.tld/ou=people,dc=example,dc=tld?cn,sn?sub?(uid=*)";
	
	LDAPClient.Search(LDAPUri,
		function(err, result) {
			if (err) {
				console.log(err);
			} else {
				console.log("Results: ");
				console.log(result);
			}
		}
	);

	/*
	LDAP URI:
	
	scheme://hostport/dn[?attrs[?scope[?filter]]]
	  scheme is the ldap scheme, either ldap, ldaps or ldapi
	  hostport is a host name with an optional ":portnumber"
	  dn is the search base
	  attrs is a comma separated list of attributes to request
	  scope is one of these three strings:
	    base one sub (default=base)
	  filter is ldap filter
	*/

Related projects
----------------

* https://github.com/joewalnes/node-ldapauth
* https://github.com/jeremycx/node-LDAP

Resources
---------

* http://nodejs.org/
* http://www.openldap.org/
* man 3 ldap_bind

*2010, xSmurf, xsmurf@smurfturf.net, http://mlalonde.net/*
