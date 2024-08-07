#include "connection.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s <config_file> <index>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int index = atoi(argv[2]) - 1;

    if(index+1 > 4 || index + 1 < 1)
    {
        fprintf(stderr, "Min id: %d\nMax id: %d\n", 1, MAX_PLAYERS);
        exit(EXIT_FAILURE);

    }

    node_t players[MAX_PLAYERS];
    load_config(argv[1], players, MAX_PLAYERS);
    int response = 0;
    network_t *net = network_config(players, MAX_PLAYERS, index);

    system("clear");

    printf("Waiting Start the game\n");
    
    getchar();
    deck_t *deck = create_deck();    
    
    /* Distribute cards */
    distribute_cards(net, deck);
    
    /* Predicitions */
    predictions(net);

    while(net->round <= NUM_ROUNDS)
    {

        /* Play cards */
        play_round(net);

        /* Check winner */
        end_round(net);

        while((net->node_id != net->card_dealer) && (response != 2))
            response = receive_packet_and_pass_forward(net);
    
        
    }

    /* Check winner */
    match_end(net);

    /* Free memory */
    free(deck->cards);  
    free(deck);
    free(net->packet);
    free(net->deck->cards);
    free(net->deck);
    close(net->socket_fd);
    free(net);


    return 0;
}


