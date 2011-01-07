LDAP Search Bindings for Node.JS
================================

This is a fork and merge of [node-LDAP](https://github.com/jeremycx/node-LDAP) and [node-ldapauth](https://github.com/joewalnes/node-ldapauth).

Provides a simple function for running search queries on an LDAP server.

It binds to the native OpenLDAP library (libldap) and calls ldap_simple_bind().

It has SSL (ldaps) and LDAP URI support.

It does not yet support LDAP bind.

It is still experimental, YMMV!

Building
--------

	node-waf configure build

Usage
-----

Ensure libldap (OpenLDAP client library) is installed.

You need to add ldap.node to your application.

	var sys		= require("sys"),
	LDAPClient	= require("./build/default/ldap.node"); // Path to ldapauth.node
	
	LDAPClient.Search("ldaps://ldap.example.tld/ou=people,dc=example,dc=tld?*?sub?uid=*",
		function(err, result) {
			if (err) {
				sys.puts(err);
			} else {
				sys.puts("Result: ");
				console.log(result);
			}
		}
	);

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
