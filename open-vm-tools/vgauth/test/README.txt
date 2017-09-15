 *    The VGAuthService smoke test.
 *
 *    This does some very basic vgauth effort to verify that it
 *    properly validates SAML tokens.  This verifies that vgauth
 *    was built and installed properly.
 *
 *    This uses a built-in SAML token with a 1000 year lifetime,
 *    to avoid any issues with the XML security library on the signing
 *    side.
 *
 *    This test must be run as root on the same system as VGAuthService.
 *    This test should only be run in a test environment, since it
 *    will clear out any existing aliases.
 *
 *    To use:
 *    - start VGAuthService (/usr/bin/VGAuithService -s )
 *    - run the smoketest
 *
 *
 *    Steps:
 *    - clear out any existing aliases
 *    - add an alias using the built-in cert
 *    - validate the SAML token
 *
 *    Possible reasons for failure:
 *    - VGAuthService wasn't started
 *    - VGAuthService failed to start up properly
 *       - unable to find support files (schemas)
 *       - unable to access various files/directories
 *       - parts of xmlsec1 missing (openssl crypto lib missing)
 *       - SAML verification failed to init (xmlsec1 build issues)
 *    - token fails to validate
 *       - this test was run after 12/18/3015
 *       - xmlsec1-config lies about how xmlsec1 was built
 *          some packages leave out -DXMLSEC_NO_SIZE_T,
 *          which can make some data structures a different size
 *          than in the library
