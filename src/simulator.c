#include "simulator.h"


double End_time = END_TIME;
Node* available;
double clock = 0.0;
double oldclock = 0.0;
double T = 0.0;
int reg_cycle_n = MIN_REG_N;

int reached_end = 0;

void simulate(System *sys)
{
    int i;
    Means means;

    /* Initialize system */
    initialize(sys);

    for (i = 0; i < 30; i++)
    {
        if (i>0){
            End_time = clock + END_TIME;
            reached_end = 0;
            oldclock = clock;
            reset_stations_measurements(sys->stations);
        }
        /* Print report (if DEBUG is ON) and THEN run */
        do {
            #ifdef DEBUG  // Print DEBUG
            system_recap(*sys);
            getchar();
            #endif
        } while (!engine(sys));

        // Update Measurements every Regeneration Cycle
        T = clock - oldclock;
        update_mean_measures(&means, sys->stations, i);
        fprintf(stderr, "waiting area = %40.20lf\n", means.squared_sum_waiting_area[1]);

        /* Final prints */
        system_recap(*sys);

        fprintf(stderr, "N_dep from Server: %d\n", sys->stations[1].measures.departures_n);
        fprintf(stderr, "N_arr to Server: %d\n", sys->stations[1].measures.arrivals_n);
        fprintf(stderr, "Final clock: %lf\n", clock);
        fprintf(stderr, "Throughput of station 1: %lf\n", sys->stations[1].measures.departures_n/T);
    }
    compute_statistics(sys, means);
    fprintf(stderr, "Mean number of Jobs at station 0: %lf\n", sys->statistics.mean_number_jobs[0]);  // TODO: CHANGE
    fprintf(stderr, "Mean number of Jobs at station 1: %lf\n", sys->statistics.mean_number_jobs[1]);
    fprintf(stderr, "Mean number of Jobs in system: %lf\n", sys->statistics.mean_number_jobs[0] + sys->statistics.mean_number_jobs[1]);
    fprintf(stderr, "Mean waiting: %lf\n", sys->statistics.mean_waiting_time[1]);
    fprintf(stderr, "Waiting semi_interval: %lf\n", sys->statistics.semi_interval_waiting_time[1]);
}

void initialize(System *sys_point)
{
    /* Initialize clock, FEL and node pool and allocate memory for stations */
    clock = 0;
    oldclock = 0;
    sys_point->event_counter = 0;
    initialize_stations(&(sys_point->stations));
    sys_point->fel = NULL;

    starting_events(&(sys_point->fel), (sys_point->stations));
    set_renewal_state(sys_point);
}

void initialize_stations(Station **pointer_to_stations)
{
    *pointer_to_stations = calloc(N_STATIONS, sizeof(Station));
    Station *stat = *pointer_to_stations;

    stat[0].type = 'D';
    stat[0].distribution = 'e';
    stat[0].parameter = 300.0;
    stat[0].prob_to_stations[0] = 0.0;
    stat[0].prob_to_stations[1] = 1.0;
    stat[0].queue.head = NULL;
    stat[0].queue.tail = NULL;
    stat[0].jobs_in_service = 0;
    stat[0].jobs_in_queue = 0;
    stat[0].server_n = 0;  // Does not apply
    stat[0].coffe_prob = 0.0;  // Does not apply
    stat[0].coffe_distribution = '\0';  // Does not apply
    stat[0].coffe_parameter = 0.0;  // Does not apply

    stat[0].measures.arrivals_n = 0;
    stat[0].measures.departures_n = 0;
    stat[0].measures.waiting_area = 0.0;


    stat[1].type = 'S';
    stat[1].distribution = 'e';
    stat[1].parameter = 40.0;
    stat[1].prob_to_stations[0] = 1.0;
    stat[1].prob_to_stations[1] = 0.0;
    stat[1].queue.head = NULL;
    stat[1].queue.tail = NULL;
    stat[1].jobs_in_service = 0;
    stat[1].jobs_in_queue = 0;
    stat[1].server_n = 1;
    stat[1].coffe_prob = 0.0;
    stat[1].coffe_distribution = 'e';
    stat[1].coffe_parameter = 10;

    stat[1].measures.arrivals_n = 0;
    stat[1].measures.departures_n = 0;
    stat[1].measures.waiting_area = 0.0;
}

void reset_stations_measurements(Station *stations)
{
    int i;
    for (i = 0; i < N_STATIONS; i++)
    {
        Measurements *measures = &(stations[i].measures);
        measures->arrivals_n = 0;
        measures->departures_n = 0;
        measures->waiting_area = 0.0;
    }
}

void starting_events(Tree *pointer_to_fel, Station *stations)
{
    int i;
    Node *new_notice;
    // Schedule 10 arrivals at the delay station
    for (i = 0; i < 10; i++)
    {
        new_notice = get_new_node(available);
        sprintf(new_notice->event.name, "J%d", i);
        new_notice->event.type = ARRIVAL;
        new_notice->event.station = 0;  // First arrival to station 0
        new_notice->event.create_time = clock;
        new_notice->event.occur_time = 0.0;
        new_notice->next = NULL;
        new_notice->previous = NULL;
        schedule(new_notice, pointer_to_fel);
    }
}

int engine(System *sys)
{
    Station *stations = sys->stations;
    Tree *pointer_to_fel = &(sys->fel);

    /* Initializations */
    int halt = 0;

    /* Get next event from FEL */
    Node* new_event = event_pop(pointer_to_fel);

    /* update clock and check if reached End_time */
    double oldtime = clock;
    double delta = update_clock(new_event, oldtime);
    if (clock >= End_time)
        reached_end = 1;

    update_stations_measurements(sys, delta);

    switch(new_event->event.type)
    {
        case ARRIVAL:
            arrival(new_event, stations, pointer_to_fel);
        break;
        case DEPARTURE:
            departure(new_event, stations, pointer_to_fel);
        break;
        case END:
            fprintf(stderr, "WHAT? END does not exist anymore.\n");
        break;
    }

    sys->event_counter++;

    if (reached_end)
    {
        if (compare_stations_state(sys->stations, sys->initialized_stations))
        {
            halt = 1;
        }
    }

    return halt;
}

void arrival(Node* node_event, Station *stations, Tree *pointer_to_fel)
{
    int station_index = node_event->event.station;
    char station_type = stations[station_index].type;

    stations[station_index].measures.arrivals_n++;

    switch (station_type)
    {
        case 'S':
            arrival_at_server(node_event, stations, pointer_to_fel);
        break;
        case 'D':
            arrival_at_delay(node_event, stations, pointer_to_fel);
        break;
    }
}

void arrival_at_delay(Node* node_event, Station *stations, Tree *pointer_to_fel)
{
    /* Increase number of jobs in serice */
    int station_index = node_event->event.station;
    stations[station_index].jobs_in_service++;

    /* Change into a departure from same station without queuing */
    node_event->event.type = DEPARTURE;
    node_event->event.occur_time = clock + station_random_time(stations, station_index);

    schedule(node_event, pointer_to_fel);  // Schedule the departure
}

void arrival_at_server(Node* node_event, Station *stations, Tree *pointer_to_fel)
{
    int station_index = node_event->event.station;
    int station_full = 0;

    /* Determine wether station is full */
    if (stations[station_index].jobs_in_service == stations[station_index].server_n)
        station_full = 1;

    node_event->event.arrival_time = clock;
    if (!station_full)
    {
        /* Process arrival at non-full server */
        stations[station_index].jobs_in_service++;  // Add a job in service

        /* Change into a departure from same station after service time */
        node_event->event.type = DEPARTURE;
        node_event->event.occur_time = clock + station_random_time(stations, station_index);

        schedule(node_event, pointer_to_fel);  // Schedule departure
    }
    else {
        /* Process arrival at full server */
        enqueue(node_event, &stations[node_event->event.station]);
        stations[station_index].jobs_in_queue++;
    }
}

void departure(Node* node_event, Station *stations, Tree *pointer_to_fel)
{
    int station_index = node_event->event.station;
    char station_type = stations[station_index].type;

    stations[station_index].measures.departures_n++;

    switch (station_type)
    {
        case 'S':
            departure_from_server(node_event, stations, pointer_to_fel);
        break;
        case 'D':
            departure_from_delay(node_event, stations, pointer_to_fel);
        break;
    }
}

void departure_from_delay(Node* node_event, Station *stations, Tree *pointer_to_fel)
{
    /* Decrease number of jobs in service */
    int station_index = node_event->event.station;
    stations[station_index].jobs_in_service--;

    /* Change into arrival in next station at same time of departure */
    node_event->event.type = ARRIVAL;
    node_event->event.station = next_station(stations, station_index);

    schedule(node_event, pointer_to_fel);  // Schedule ARRIVAL
}


void departure_from_server(Node* node_event, Station *stations, Tree *pointer_to_fel)
{
    int station_index = node_event->event.station;
    double coffe_length = 0;

    stations[station_index].jobs_in_service--;  // Decrease the number of jobs in service for the station

    /* If there is something in queue for the station, schedule the arrival at same station */
    Node* next_job;
    if (stations[station_index].queue.tail) {
        /* Process departure from a server with a queue by dequeuing and immedeatly scheduling another departure from the same server */
        next_job = dequeue(&stations[node_event->event.station]);
        stations[station_index].jobs_in_queue--;

        /* Process arrival at non-full server (this is sure since there has just been a departure) */
        stations[station_index].jobs_in_service++;  // Add a job in service

        /* In case of a coffe break, schedule a longer repair to take the pause into account */
        coffe_length = coffe_break(stations, station_index);

        /* Schedule a departure from same station after service time and maybe coffe break */
        next_job->event.type = DEPARTURE;
        next_job->event.occur_time = clock + coffe_length + station_random_time(stations, station_index);
        schedule(next_job, pointer_to_fel);
    }

    /* Change into arrival at next station */
    node_event->event.type = ARRIVAL;
    node_event->event.station = next_station(stations, station_index);

    schedule(node_event, pointer_to_fel);  // Schedule arrival to next station
}

double update_clock(Node* new_event, double oldtime)
{
    double delta = 0.0;
    clock = new_event->event.occur_time;
    delta = clock - oldtime;
    return delta;
}

int next_station(Station *stations, int current_station)
{
    int i;
    SelectStream(255);  // Set separate stream for extraction of next station (255 since it's the last - and of course free - stream)
    double extraction = Uniform(0.0, 1.0);
    double *prob_to_stations = stations[current_station].prob_to_stations;

    double cumulative_prob = 0.0;

    for (i = 0; i < N_STATIONS; i++)
    {
        cumulative_prob += prob_to_stations[i];
        if (extraction <= cumulative_prob)
            return i;
        //if (approx_equal(stations[current_station].prob_to_stations[i], 1.0))
            //return i;
    }
    return -1;
}

double station_random_time(Station *stations, int station_index)
{
    double service_time = 0;

    /* Select separate stream for each station arrival */
    SelectStream(station_index);

    switch (stations[station_index].distribution)
    {
        case 'e':
            service_time = Exponential(stations[station_index].parameter);
        break;
    }
    return service_time;
}

double coffe_break(Station *stations, int station_index)
{
    double coffe_length = 0;

    SelectStream(station_index + N_STATIONS);  // Avoid overlappings
    if (Uniform(0, 1) < stations[station_index].coffe_prob)
    {
        switch (stations[station_index].coffe_distribution)
        {
            case 'e':
                SelectStream(station_index + 2*N_STATIONS);  // Avoid overlappings (second level)
                coffe_length = Exponential(stations[station_index].coffe_parameter);
            break;
        }

    }
    return coffe_length;
}

void copy_stations(Station *stations, Station **new_stations_address)
{
    int i;
    Station *stat = stations;

    *new_stations_address = calloc(N_STATIONS, sizeof(Station));

    for (i = 0; i < N_STATIONS; i++)
    {
        (*new_stations_address)[i].type =                   stat[i].type;
        (*new_stations_address)[i].distribution =           stat[i].distribution;
        (*new_stations_address)[i].parameter =              stat[i].parameter;
        (*new_stations_address)[i].prob_to_stations[0] =    stat[i].prob_to_stations[0];
        (*new_stations_address)[i].prob_to_stations[1] =    stat[i].prob_to_stations[1];
        (*new_stations_address)[i].queue.head =             stat[i].queue.head;
        (*new_stations_address)[i].queue.tail =             stat[i].queue.tail;
        (*new_stations_address)[i].jobs_in_service =        stat[i].jobs_in_service;
        (*new_stations_address)[i].jobs_in_queue =          stat[i].jobs_in_queue;
        (*new_stations_address)[i].measures.arrivals_n =    stat[i].measures.arrivals_n;
        (*new_stations_address)[i].measures.departures_n =  stat[i].measures.departures_n;
        (*new_stations_address)[i].server_n =               stat[i].server_n;
        (*new_stations_address)[i].coffe_prob =             stat[i].coffe_prob;
        (*new_stations_address)[i].coffe_distribution =     stat[i].coffe_distribution;
        (*new_stations_address)[i].coffe_parameter =        stat[i].coffe_parameter;
    }

}

int compare_stations_state(Station *s1, Station *s2)
{
    int i;
    int equal = 1;

    for (i = 0; i < N_STATIONS; i++)
    {
        if ( (s1[i].jobs_in_service != s2[i].jobs_in_service) ||
             (s1[i].jobs_in_queue != s2[i].jobs_in_queue)
           ) {
            equal = 0;
        }
    }
    return equal;
}

void set_renewal_state(System *sys_point)
{
    copy_stations(sys_point->stations, &(sys_point->initialized_stations));
    sys_point->initialized_stations[0].jobs_in_service = 10;
    sys_point->initialized_stations[0].jobs_in_queue = 0;
    sys_point->initialized_stations[1].jobs_in_service = 0;
    sys_point->initialized_stations[1].jobs_in_queue = 0;
}
