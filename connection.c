#include "connection.h"


/* Create a UDP DATAGRAM socket */
int create_socket()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    return sock;
}

/* Configure node network */
network_t *network_config(node_t *players, int num_players, int index)
{
    network_t *net = (network_t *)malloc(sizeof(network_t));
    if (!net)
    {
        perror("Failed to allocate memory for network");
        exit(EXIT_FAILURE);
    }

    net->socket_fd = create_socket();
    net->num_nodes = num_players;
    net->players = players;
    net->node_id = players[index].id;
    net->card_dealer = 1;
    net->round = 1;
    net->last_winner = -1;
    memset(net->predictions, 0, sizeof(net->predictions));
    for(int i = 0; i < MAX_PLAYERS; i++)
        net->lifes[i] = 13;
    memset(net->score, 0, sizeof(net->score));
    net->packet = init_packet();
    init_deck_player(net);


    memset(&net->current_node_addr, 0, sizeof(net->current_node_addr));
    memset(&net->next_node_addr, 0, sizeof(net->next_node_addr));
    
    /* Setting address family */
    net->current_node_addr.sin_family = AF_INET;
    net->current_node_addr.sin_addr.s_addr = inet_addr(players[index].ip);
    net->current_node_addr.sin_port = htons(players[index].port);

    if(bind(net->socket_fd, (struct sockaddr *)&net->current_node_addr, sizeof(net->current_node_addr)) < 0) {
        perror("Failed to bind socket");
        return NULL;
    }

    net->next_node_addr.sin_family = AF_INET;
    net->next_node_addr.sin_addr.s_addr = inet_addr(players[index].next_ip);
    net->next_node_addr.sin_port = htons(players[index].next_port);

    if (strcmp(players[index].ip, players[index].next_ip) == 0)
        if(players[index].port == players[index].next_port)
        {
            perror("I'm the only node in the network");
            return NULL;
        }

    if(players[index].id == 1)
        net->token = 1;
    else
        net->token = 0;


    return net;
} 


/* load config file */
void load_config(const char* filename, node_t *players, int num_players)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Unable to open config file");
        exit(EXIT_FAILURE);
    }

    int count = 0;
    
    while (fscanf(file, "%d %s %d %s %d", &players[count].id, players[count].ip, &players[count].port, players[count].next_ip, &players[count].next_port) != EOF) {
        if (players->id == num_players) {
            fclose(file);
            return;
        }
        count++;
    }

    fclose(file);
}

/* */
packet_t *init_packet()
{
    packet_t * packet;

    if((packet = malloc(sizeof(packet_t))) == NULL)
    {
        perror("Failed to allocate memory for packet");
        exit(EXIT_FAILURE);
    }
    packet->destination = 0;
    packet->type = 0;
    memset(packet->data, 0, sizeof(packet->data));
    packet->receive_confirmation = 0;
    
    return packet;
}

// packet_t *create_or_modify_packet(packet_t *p, char *origin_addr, char *destination_addr, int card, int type)
packet_t *create_or_modify_packet(packet_t *p, int destination, uint8_t *data, int type)
{

    // struct in_addr ip;
    
    if(p == NULL)
    {
        if((p = malloc(sizeof(packet_t))) == NULL)
        {
            perror("Failed to allocate memory for packet");
            exit(EXIT_FAILURE);
        }
    }
    
    memset(p, 0, sizeof(packet_t));
    p->destination = destination;
    memcpy(p->data, data, sizeof(p->data));
    p->type = type;
    p->receive_confirmation = 0;

    return p;
}


/* Verify what node has the token */
int has_token(network_t *net)
{
    if(net->token == 1)
        return 1;
    else
        return 0;
}

/* Send a packet to the next node */
int send_packet(network_t *net, packet_t *packet)
{
    sendto(net->socket_fd, packet, sizeof(packet_t), 0, (struct sockaddr *)&net->next_node_addr, sizeof(net->next_node_addr));
    return 1;
}


int send_packet_and_wait(network_t *net, packet_t *response, packet_t *packet)
{

    send_packet(net, packet);

    while(receive_packet(net, response))
    {
        switch (response->type)
        {
        case SEND_CARD:
                
            break;

        case MAKE_PREDICTION:
            for(int i = 0; i < net->num_nodes; i++)
                net->predictions[i] = response->data[i];
            break;
        case SHOW_PREDICTION:
            printf("\n****** PREDICTIONS ******\n");
            for(int i = 0; i < net->num_nodes; i++)
                printf("Player %d predicted %d\n", i+1, response->data[i]);
            printf("*************************\n");
            break;
        case PLAY_CARD:
            show_played_card(net);
            net->last_winner = calculate_results(net);
            break;
        case END_ROUND:
            show_round_results(net);
            net->card_dealer = (net->card_dealer % net->num_nodes) + 1;
            break;
        case END_MATCH:
            return 2;
            break;
        default:
                printf("Default\n"); 
            break;
        }
        if(response->receive_confirmation == 1)
        {
            return 1;
        }
    }

    return 1;

}

/* Receive a packet from the current node */
int receive_packet(network_t *net, packet_t *packet)
{   
    socklen_t aux;
    aux = sizeof(net->current_node_addr);

    while(!recvfrom(net->socket_fd, packet, sizeof(packet_t), 0, (struct sockaddr *)&net->current_node_addr, &aux))
    {
        printf("POAAAAA");
    }

    return 1;
}

/* Receive a packet from the current node and pass it forward */
int receive_packet_and_pass_forward(network_t *net)
{
    int value, suit;
    uint8_t data[4];
    while(1)
    {
        receive_packet(net, net->packet);

        int target_node = (net->node_id % net->num_nodes); 
        memcpy(data, net->packet->data, sizeof(data));
        if(net->packet->destination == net->node_id)
        {    

            switch (net->packet->type)
            {
                case SEND_CARD:
                    if(net->deck->size == NUM_CARDS + 1 - net->round)
                        break;
                    retrieve_card(net->packet->data[0], &value, &suit);
                    net->deck->cards[net->deck->size].suit = suit;
                    net->deck->cards[net->deck->size].value = value;
                    print_card(net->deck->cards[net->deck->size]);
                    net->deck->size++;
                break;
                case MAKE_PREDICTION:
                    data[net->node_id-1] = calculate_prediction(net);
                    create_or_modify_packet(net->packet, net->players[target_node].id, data, MAKE_PREDICTION);
                break;

                case SHOW_PREDICTION:
                    printf("\n**** PREDICTIONS ****\n");
                    for(int i = 0; i < net->num_nodes; i++)
                    {
                        printf("Player %d predicted %d\n", i+1, net->packet->data[i]);
                        net->predictions[i] = net->packet->data[i];
                    }
                    printf("*********************\n");
                    create_or_modify_packet(net->packet, net->players[target_node].id, data, SHOW_PREDICTION);
                break;

                case PLAY_CARD:
                    show_played_card(net);
                    card_t card = get_card(net->deck);
                    set_card(&data[net->node_id-1], card.value, card.suit);
                    create_or_modify_packet(net->packet, net->players[target_node].id, data, PLAY_CARD);
                break;

                case END_ROUND:
                    show_round_results(net);
                    net->last_winner = data[0];
                    net->score[net->last_winner-1]++;
                    net->round++;
                    create_or_modify_packet(net->packet, net->players[target_node].id, data , END_ROUND);
                    net->card_dealer = (net->card_dealer % net->num_nodes) + 1;
                break;

                case END_MATCH:
                    memset(data, 0, sizeof(data));
                    create_or_modify_packet(net->packet, net->players[target_node].id, data , END_MATCH);
                    send_packet(net, net->packet);
                    return 2;
                break;

                case SEND_TOKEN:
                    net->token = 1;
                    printf("Player %d received token\n", net->node_id);
                    return 1;
                break;

                default:

                break;
            }
            net->packet->receive_confirmation = 1;
        }
        memset(data, 0, sizeof(data));
        send_packet(net, net->packet);

    }

    return 1;
}


/********************************************* Game Functions  **************************************************/


/* End the match, show the winner of the match */
void match_end(network_t *net)
{
    uint8_t data[4];
    memset(data, 0, sizeof(data));
    printf("+++++ MATCH ENDED +++++\n");
    for(int i = 0; i < net->num_nodes; i++)
    {
        net->lifes[i] = net->lifes[i] - abs(net->predictions[i] - net->score[i]);
        printf("Player %d predicted %d and scored %d\n", i+1, net->predictions[i], net->score[i]);
    }
    for(int i = 0; i < net->num_nodes; i++)
    {
        printf("Player %d lifes: %d\n", i+1, net->lifes[i]);
    }
    
    int max_life = -1;
    int player = -1;
    for(int i = 0; i < net->num_nodes; i++)
    {
        if(net->lifes[i] > max_life)
        {

            max_life = net->lifes[i];
            player = i+1;
        }
        else if(net->lifes[i] == max_life)
        {
            if(net->predictions[i] > net->predictions[player-1])
                player = i+1;
        }
    }

    printf("Player %d wins the match\n", player);
    printf("++++++++++++++++++++++\n");
    if(net->node_id == net->card_dealer)
    {
        data[0] = player;
        int target_node = (net->node_id) % net->num_nodes;
        create_or_modify_packet(net->packet, net->players[target_node].id, data, END_MATCH);
        send_packet_and_wait(net, net->packet, net->packet);
    }
}

/* End the round, show the winner of the round and pass the token */
void end_round(network_t *net)
{
    if(net->node_id == net->card_dealer)
    {
        uint8_t data[4];
        memset(data, 0, sizeof(data));
        int target_node = (net->node_id) % net->num_nodes;
        data[0] = net->last_winner;
        create_or_modify_packet(net->packet,net->players[target_node].id, data, END_ROUND);
        net->score[net->last_winner-1]++;
        send_packet_and_wait(net, net->packet, net->packet);
        net->round++;
        pass_token(net);
    }
}

/* Play a round of the game */
void play_round(network_t *net)
{
    if(net->node_id == net->card_dealer)
    {
        uint8_t data[4];
        memset(data, 0, sizeof(data));
        card_t card = get_card(net->deck);
        set_card(&data[net->node_id-1], card.value, card.suit);
        int target_node = (net->node_id % net->num_nodes);
        create_or_modify_packet(net->packet, net->players[target_node].id, data, PLAY_CARD);
        send_packet_and_wait(net, net->packet, net->packet);
    }   

}

/* Show the results of the round */
void show_round_results(network_t *net)
{
    printf("#### ROUND %d WINNER ####\n", net->round);
    printf("player %d wins the round\n", net->packet->data[0]);
    printf("########################\n");
}

/* Calculate the winner of the round */
int calculate_results(network_t *net)
{
    card_t card;
    card_t max;
    int winner = -1;

    max.value = 0;
    max.suit = 0;

    for(int i = 0; i < net->num_nodes; i++)
    {
        retrieve_card(net->packet->data[i], &card.value, &card.suit);
        if(card.value > max.value)
        {
            max.value = card.value;
            max.suit = card.suit;
            winner = i+1;
        }
        else if(card.value == max.value)
        {
            if(card.suit > max.suit)
            {
                max.value = card.value;
                max.suit = card.suit;
                winner = i+1;
            }
        }        
    }

    return winner;
}

/* Get a card of the deck to play */
card_t get_card(deck_t *deck)
{
    card_t card;
    card = deck->cards[0];

    for(int i = 0; i < deck->size-1; i++)
        deck->cards[i] = deck->cards[i+1];

    deck->size--;

    return card;
}

/* Show player cards of the round */
void show_played_card(network_t *net)
{
    card_t card;
    int i;
    i = net->card_dealer;

    if(net->node_id == net->card_dealer)
        for(int i = 0; i < net->num_nodes; i++)
        {
            printf("Player %d played: ", i+1);
            retrieve_card(net->packet->data[i], &card.value, &card.suit);
            print_card(card);
            printf("\n");
        }
    else
    {
        while(i != net->node_id)
        {
            printf("Player %d played ", i);
            retrieve_card(net->packet->data[i-1], &card.value, &card.suit);
            print_card(card);
            printf("\n");
            i = (i % net->num_nodes) + 1;
        }
    }

}

/* Initialize the deck of the player */
void init_deck_player(network_t *net)
{
    if((net->deck = malloc(sizeof(deck_t))) == NULL)
    {
        perror("Failed to allocate memory for deck");
        exit(EXIT_FAILURE);
    }

    if((net->deck->cards = malloc(sizeof(card_t) * NUM_PLAYER_CARDS)) == NULL)
    {
        perror("Failed to allocate memory for cards");
        exit(EXIT_FAILURE);
    }

    net->deck->size = 0;

    return;

}

/* Create a deck of cards */
deck_t *create_deck()
{

    deck_t *deck;

    if((deck = (deck_t *)malloc(sizeof(deck_t))) == NULL)
    {
        perror("Failed to allocate memory for deck");
        exit(EXIT_FAILURE);
    }

    if((deck->cards = (card_t *)malloc(sizeof(card_t) * NUM_CARDS)) == NULL )
    {
        perror("Failed to allocate memory for cards");
        exit(EXIT_FAILURE);
    }

    deck->size = NUM_CARDS;

    for(int i = 0; i < NUM_CARDS; i++)
    {
        deck->cards[i].value = (i%SEQ_TOTAL) + 1;
        deck->cards[i].suit = i/SEQ_TOTAL;
    }

    return deck;

}

/* Shuffle the deck */
int shuffle_deck(deck_t *deck)
{
    int i, j;
    card_t aux;

    for(i = 0; i < deck->size; i++)
    {
        j = rand() % deck->size;
        aux = deck->cards[i];
        deck->cards[i] = deck->cards[j];
        deck->cards[j] = aux;
    }

    return 1;
}

/* Distribute cards to the players */
void distribute_cards(network_t *net, deck_t *deck)
{
    if(!has_token(net))
        return;

    shuffle_deck(deck);

    int cards_per_player = NUM_CARDS / net->num_nodes;
    int count = 0;
    card_t card; 
    packet_t *packet = init_packet();
    packet_t *response = init_packet();
    uint8_t data[4];
    memset(data, 0, sizeof(data));

    for(int j = 0; j < cards_per_player; j++)
    {
        for(int k = 1; k < net->num_nodes; k++)
        {
            int target_node = (net->node_id - 1 + k) % net->num_nodes; 
            card = deck->cards[count];
            count++;
            
            set_card(&data[0], card.value, card.suit);
            packet = create_or_modify_packet(packet,net->players[target_node].id, data, SEND_CARD);
            send_packet_and_wait(net, response, packet);
        }
        card = deck->cards[count];
        count++;
        net->deck->cards[j] = card;
        net->deck->size++;
        print_card(card);
    }
    free(packet);
    free(response);
}

/* Set card with value and suit */
void set_card(uint8_t *card, int value, int suit) {
     if (value < 0 || value > 13){
        printf("Value must be betwenn 0 and 13\n");
        return;
    }
    if (suit < 0 || suit > 3) {
        printf("Suit value must be betwenn 0 and 3\n");
        return;
    }

    *card = (value & CARD_VALUE_MASK) << 2 | (suit & CARD_SUIT_MASK);

}

/* Retrieve value and suit from card */
void retrieve_card(uint8_t card, int *value, int *suit) {
    *value = (card >> 2) & CARD_VALUE_MASK; 
    *suit = card & CARD_SUIT_MASK;        
}

/* Print card */
void print_card(card_t card)
{
    char *suit_symbol;
    char value, aux;

    switch(card.suit)
    {
        case 3: suit_symbol = "♣"; break;
        case 2: suit_symbol = "♥"; break;
        case 1: suit_symbol = "♠"; break;
        case 0: suit_symbol = "♦"; break;
        default: suit_symbol = "?"; break;
    }
    switch(card.value)
    {
        case 1: value = '4'; break;
        case 2: value = '5'; break;
        case 3: value = '6'; break;
        case 4: value = '7'; break;
        case 5: value = '8'; break;
        case 6: value = '9'; break;
        case 7: value = '1', aux = '0'; break;
        case 8: value = 'J'; break;
        case 9: value = 'Q'; break;
        case 10: value = 'K'; break;
        case 11: value = 'A'; break;
        case 12: value = '2'; break;
        case 13: value = '3'; break;  
    }

    if(card.value == 7)
        printf("%c%c%s ", value, aux, suit_symbol);
    else
        printf("%c%s ", value , suit_symbol);
    
}

/* Calculate the prediction of the player */
int calculate_prediction(network_t *net)
{
    int prediction = 1;

    for(int i = 0; i < net->deck->size; i++)
    {
        if(net->deck->cards[i].value >= 9)
            prediction++;
    }

    if(prediction > 1 )
        return prediction - 1;

    return prediction;
}


/* Calculate the prediction of the player */
void predictions(network_t *net)
{

    if(net->node_id == net->card_dealer)
    {

        net->predictions[net->node_id-1] = calculate_prediction(net);
        int target_node = (net->node_id % net->num_nodes);
        create_or_modify_packet(net->packet, net->players[target_node].id, net->predictions, MAKE_PREDICTION);
        send_packet_and_wait(net, net->packet, net->packet);
        create_or_modify_packet(net->packet, net->players[target_node].id, net->predictions, SHOW_PREDICTION);
        send_packet_and_wait(net, net->packet, net->packet);
    }
        
}

/* Pass the token to the next node */
void pass_token(network_t *net)
{

    net->token = 0;
    uint8_t data[4];
    memset(&data, 0, sizeof(data));
    if(net->round < NUM_ROUNDS)
        printf("The card dealer now is player: %d\n", (net->node_id % net->num_nodes) + 1);
    create_or_modify_packet(net->packet, net->players[net->node_id % net->num_nodes].id, data, SEND_TOKEN);
    send_packet(net, net->packet);

}