#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <jack/jack.h>
/* #include <jack/transport.h> */

jack_client_t *client;
jack_port_t* input_port;

#define N_OUTPUT_PORTS 11   /* XXX: Dynamically allocate */
jack_port_t* output_ports[N_OUTPUT_PORTS];

static int process(jack_nframes_t nframes, void *arg) {
  return 0;
}

static void signal_handler(int sig) {
  jack_client_close(client);
  fprintf(stderr, "signal received, exiting ...\n");
  exit(0);
}

int main (int argc, char *argv[]) {
  jack_status_t status;

  if(!(client = jack_client_open("LOO.C", JackNoStartServer, &status))) {
    fprintf(stderr, "jack server not running\n");
    return 1;
  }

  jack_set_process_callback (client, process, 0);

  int i;
  for(i=0; i<N_OUTPUT_PORTS; ++i) {
    char port_name[64];
    sprintf(port_name, "loop_%d", i+1);
    output_ports[i] = jack_port_register (client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  }

  if(jack_activate(client)) {
    fprintf(stderr, "cannot activate client, whatever that means\n");
    return 1;
  }

  /* gracefully quit, upon kindly request */
  /* XXX: apparently `signal' is deprecated for sigaction (?) */
  /* XXX: also, WIN32 may use different signals (?) */
  signal(SIGQUIT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGINT, signal_handler);

  while (1) {
    sleep(1);
  };

  /* how can we ever get here? */
  fprintf(stderr, "really? you found me here?\n");
  jack_client_close(client);
}
