#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

/**
 * Simple log function
 */
void slog(char* message) {
    fprintf(stdout, message);
}

/**
 * Print SSL error details
 */
void print_ssl_error(char* message, FILE* out) {

    fprintf(out, message);
    fprintf(out, "Error: %s\n", ERR_reason_error_string(ERR_get_error()));
    fprintf(out, "%s\n", ERR_error_string(ERR_get_error(), NULL));
    ERR_print_errors_fp(out);
}

/**
 * Print SSL error details with inserted content
 */
void print_ssl_error_2(char* message, char* content, FILE* out) {

    fprintf(out, message, content);
    fprintf(out, "Error: %s\n", ERR_reason_error_string(ERR_get_error()));
    fprintf(out, "%s\n", ERR_error_string(ERR_get_error(), NULL));
    ERR_print_errors_fp(out);
}

/**
 * Initialise OpenSSL
 */
void init_openssl() {

    /* call the standard SSL init functions */
    SSL_load_error_strings();
    SSL_library_init();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();

    /* seed the random number system - only really nessecary for systems without '/dev/random' */
    /* RAND_add(?,?,?); need to work out a cryptographically significant way of generating the seed */
}


/**
 * Connect to a host using an encrypted stream
 */
BIO* connect_encrypted(char* host_and_port, char* store_path, SSL_CTX** ctx, SSL** ssl) {

    BIO* bio = NULL;
    int r = 0;

    /* Set up the SSL pointers */
//    *ctx = SSL_CTX_new(TLSv1_client_method());
    *ctx = SSL_CTX_new(TLS_client_method());
    *ssl = NULL;
  //      r = SSL_CTX_load_verify_locations(*ctx, store_path, NULL);
        r = SSL_CTX_load_verify_locations(*ctx, NULL, store_path);

    if (r == 0) {

        print_ssl_error_2("Unable to load the trust store from %s.\n", store_path, stdout);
        return NULL;
    }

    /* Setting up the BIO SSL object */
    bio = BIO_new_ssl_connect(*ctx);
    BIO_get_ssl(bio, ssl);
    if (!(*ssl)) {

        print_ssl_error("Unable to allocate SSL pointer.\n", stdout);
        return NULL;
    }
    SSL_set_mode(*ssl, SSL_MODE_AUTO_RETRY);

    /* Attempt to connect */
    BIO_set_conn_hostname(bio, host_and_port);

    /* Verify the connection opened and perform the handshake */
    if (BIO_do_connect(bio) < 1) {

        print_ssl_error_2("Unable to connect BIO.%s\n", host_and_port, stdout);
        return NULL;
    }

    if (SSL_get_verify_result(*ssl) != X509_V_OK) {
		fprintf (stdout, "result %d\n", SSL_get_verify_result(*ssl));
        print_ssl_error("Unable to verify connection result.\n", stdout);
    }

    return bio;
}

/**
 * Read a from a stream and handle restarts if nessecary
 */
ssize_t read_from_stream(BIO* bio, char* buffer, ssize_t length) {

    ssize_t r = -1;

    while (r < 0) {

        r = BIO_read(bio, buffer, length);
        if (r == 0) {

            print_ssl_error("Reached the end of the data stream.\n", stdout);
            continue;

        } else if (r < 0) {

            if (!BIO_should_retry(bio)) {

                print_ssl_error("BIO_read should retry test failed.\n", stdout);
                continue;
            }

            /* It would be prudent to check the reason for the retry and handle
             * it appropriately here */
        }

    };

    return r;
}

/**
 * Write to a stream and handle restarts if nessecary
 */
int write_to_stream(BIO* bio, char* buffer, ssize_t length) {

    ssize_t r = -1; 

    while (r < 0) {

        r = BIO_write(bio, buffer, length);
        if (r <= 0) {

            if (!BIO_should_retry(bio)) {

                print_ssl_error("BIO_read should retry test failed.\n", stdout);
                continue;
            }

            /* It would be prudent to check the reason for the retry and handle
             * it appropriately here */
        }

    }

    return r;
}

/**
 * Main SSL demonstration code entry point
 */
int main() {

    char* host_and_port = "oauth2.googleapis.com:443"; 
    char* server_request = "POST /device/code HTTP/1.1\r\n"
		"Host: oauth2.googleapis.com\r\n"
		"Content-Type: application/x-www-form-urlencoded\r\n"
		"Content-Length: 153\r\n"
		"\r\n"
		"client_id%3D190164581320-s44milsm279lmph523v5d8b4uo33u3lo.apps.googleusercontent.com&"
		"scope%3Dhttps%3A%2F%2Fwww.googleapis.com%2Fauth%2Fyoutube.readonly\r\n"; 
//    char* store_path = "/etc/ssl/mycerts/testhttps.pem"; 
//    char* store_path = "/etc/ssl/certs/VeriSign_Universal_Root_Certification_Authority.pem"; 
    char* store_path = "/etc/ssl/certs"; 
    char buffer[4096];
    buffer[0] = 0;

    BIO* bio;
    SSL_CTX* ctx = NULL;
    SSL* ssl = NULL;

    /* initilise the OpenSSL library */
    init_openssl();

        if ((bio = connect_encrypted(host_and_port, store_path, &ctx, &ssl)) == NULL)
            return (EXIT_FAILURE);


    write_to_stream(bio, server_request, strlen(server_request));
    read_from_stream(bio, buffer, 8192);
    printf("%s\r\n", buffer);

     /* clean up the SSL context resources for the encrypted link */
        SSL_CTX_free(ctx);

    return (EXIT_SUCCESS);
}
