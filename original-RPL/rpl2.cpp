/*
 * Simple Wiselib Example
 */
#include "external_interface/external_interface.h"
#include "algorithms/routing/tree/tree_routing.h"


#define NEIGHBOR_SIZE 255 //65534 is the maximum value. do NOT set this bigger than 255 never!
#define SENT_QUEUE 254      //254 is the maximum value. do NOT set this bigger than 255 never!

#define OF_TO_USE 0x00
#define MIN_HIGH_TREE 0x00

#define NEIGHBOR_DIS 0
#define NEIGHBOR_DIS_ACK 1
#define DIO 2
#define DAO 3

typedef wiselib::OSMODEL Os;

class rpl
{
public:



    struct metrics
    {
        uint16_t metric1;
        uint16_t metric2;
        uint16_t metric3;
        uint16_t metric4;
    };

    typedef metrics metrics_t;

    struct rpl_dio
    {
        uint16_t parent;
        uint8_t type;
        uint8_t hops;
        uint16_t id;
        uint8_t version;
        uint8_t dest;
        uint8_t given;
        uint16_t rank;
        uint8_t instance_id;
        char from[32];
        char Routing[32];
        metrics_t dio_metrics;

    };
    typedef rpl_dio rpl_dio_t;

    struct header
    {
        uint8_t type;
        uint16_t to_node;
        uint16_t from_node;
       // uint16_t to_ipv6[8];
      //  uint16_t from_ipv6[8];
    };
    typedef header header_t;

    struct rpl_dag
    {
        uint8_t of;
        uint16_t parent;
        uint8_t version;
        uint8_t id;
        uint8_t instance_id;
        uint8_t joined;
        uint8_t hops;
        uint16_t rank;
        metrics_t dag_metrics;
    };
    typedef rpl_dag rpl_dag_t;


    struct rpl_dao
    {
        uint16_t to;
        uint16_t from;
        uint8_t instance_id;
        uint8_t dao_sequence;
    };

    typedef rpl_dao rpl_dao_t;

    void init( Os::AppMainParameter& value )
    {
        radio_ = &wiselib::FacetProvider<Os, Os::Radio>::get_facet( value );
        timer_ = &wiselib::FacetProvider<Os, Os::Timer>::get_facet( value );
        debug_ = &wiselib::FacetProvider<Os, Os::Debug>::get_facet( value );



        radio_->reg_recv_callback<rpl,
        &rpl::receive_radio_message>( this );

        timer_->set_timer<rpl, &rpl::discover_neighbors>( 2000, this, 0 );

        rpl_dag_structure.rank=0xffff;
        rpl_dag_structure.dag_metrics.metric1=0xffff;
        rpl_dag_structure.of=OF_TO_USE;
       for(uint16_t j=0;j<NEIGHBOR_SIZE; j++)
       {
            for (uint16_t i=0;i<NEIGHBOR_SIZE; i++)
            {
                // debug_->debug("node: %d value: %d\n",radio_->id(), i);
                neighbors[j][i]=0xffff;
            }
       }

        for (uint8_t i=0;i<SENT_QUEUE;i++)
        {
            sent_timer_[i]=0xff;
        }

        sequence_counter_=0x00;

        if(radio_->id()==0)
        {
            rpl_dag_structure.rank=0;

            rpl_dag_structure.dag_metrics.metric1=0;
            rpl_dag_structure.version=5;

        }
        if(radio_->id()==6)
        {
            timer_->set_timer<rpl, &rpl::send_dao>( 15000, this, 0 );
        }

                    timer_->set_timer<rpl,
        &rpl::dio_output>( 5000, this, 0 );

    }
//--------------------------------------------------------------------------------------------------------------------
  //  void discover_neighbors( void* )
//--------------------------------------------------------------------------------------------------------------------
    void discover_neighbors( void* )
    {
        header_t my_header;
        my_header.type=NEIGHBOR_DIS;
        my_header.from_node=radio_->id();
        radio_->send( Os::Radio::BROADCAST_ADDRESS, sizeof(header_t), ( Os::Radio::block_data_t*)&my_header );

    }

//--------------------------------------------------------------------------------------------------------------------

    void handle_neighbor_discovery (header_t inbox_header)
    {
      //  debug_->debug("node %d received nd from node %d\n",radio_->id(), inbox_header.from_ipv6[0]);
        header_t header;
        header.type=NEIGHBOR_DIS_ACK;
       // header.to_ipv6[0]=inbox_header.from_ipv6[0];
       // header.from_ipv6[0]=radio_->id();
        header.to_node=inbox_header.from_node;
        header.from_node=radio_->id();
        radio_->send( Os::Radio::BROADCAST_ADDRESS, sizeof(header_t), ( Os::Radio::block_data_t*)&header );
    }
//--------------------------------------------------------------------------------------------------------------------

    void handle_neighbor_discovery_ack(header_t inbox_header)
    {
        bool used=false;
        uint16_t position=0xffff;
        for (uint16_t i=0; i<NEIGHBOR_SIZE; i++)
        {
            if (position==0xffff && neighbors[i][0]==0xffff)
            {
                position=i;
            }
            if(neighbors[i][0]==inbox_header.from_node)
            {
                used=true;
                break;
            }
        }
        if(used==false)
        {
            neighbors[position][0]=inbox_header.from_node;
            debug_->debug("node: %d added %d to list\n", radio_->id(), inbox_header.from_node);
        }
    }
//--------------------------------------------------------------------------------------------------------------------
void dio_output (void *)
{
    rpl_dio_t dio;

    dio.id = radio_->id();
    dio.dio_metrics = rpl_dag_structure.dag_metrics;
    dio.version = rpl_dag_structure.version;
    dio.instance_id = rpl_dag_structure.instance_id;

    header_t header;
    header.type=DIO;
    uint16_t size;
    size = sizeof(header_t)+sizeof(rpl_dio_t);
    unsigned char message[size];
    memcpy(message, &header, sizeof(header_t));
    memcpy(message+sizeof(header_t),&dio, sizeof(rpl_dio_t));
   // debug_->debug("metric1: %d, version: %d\n", dio.dio_metrics.metric1, dio.version);

    radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);

    timer_->set_timer<rpl, &rpl::dio_output>( 2000, this, 0 );

}

//--------------------------------------------------------------------------------------------------------------------
void dio_input (rpl_dio_t input_dio)
{
    //debug_->debug("node: %d, metric1: %d, version: %d\n", radio_->id(), input_dio.dio_metrics.metric1, input_dio.version);
    switch(rpl_dag_structure.of)
    {
        case MIN_HIGH_TREE:
            temp_rank=min_high_tree_OF(input_dio.dio_metrics);
            if(temp_rank<rpl_dag_structure.rank)
            {
                rpl_dag_structure.rank = temp_rank;
                rpl_dag_structure.hops = input_dio.hops + 1;
                rpl_dag_structure.parent = input_dio.id;
                rpl_dag_structure.version = input_dio.version;
                rpl_dag_structure.dag_metrics.metric1 = input_dio.dio_metrics.metric1+1;
                debug_->debug("node %d (metric1= %d) joined to parrent %d (metric1= %d)\n", radio_->id(),rpl_dag_structure.dag_metrics.metric1, rpl_dag_structure.parent, input_dio.dio_metrics.metric1);

            }
        break;
    }
}
//--------------------------------------------------------------------------------------------------------------------
void dao_output (rpl_dag_t input_dag, uint16_t dest, uint16_t from)
{



   for (sequence_counter_=0;sequence_counter_<SENT_QUEUE;sequence_counter_++)
        {
            if(sent_timer_[sequence_counter_]==0xff)
            {
                break;
            }
        }
    if (sequence_counter_!=0xff)
    {
        header_t header;
        rpl_dao_t dao;

        header.type=DAO;
        header.from_node = radio_->id();
        header.to_node = input_dag.parent;

        dao.to=dest;
        dao.from=from;


        dao.instance_id = input_dag.instance_id;
        dao.dao_sequence = sequence_counter_;

        uint16_t size;
        size = sizeof(header_t)+sizeof(rpl_dao_t);
        unsigned char message[size];
        memcpy(message, &header, sizeof(header_t));
        memcpy(message+sizeof(header_t),&dao, sizeof(rpl_dao_t));
        radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);

    }


}
//--------------------------------------------------------------------------------------------------------------------
void dao_input(header_t header, rpl_dao_t input_dao)
{
   // if(input_dao.to==radio_->id())                      //einai gia tin parousa diergasia to dao?
  //  {
        //handle dao o.O
 //   }
 //   else
  //  {
        uint16_t position;

        for (position=0;position<NEIGHBOR_SIZE; position++)  // briskei ton geitona pou to esteile
        {
            if(neighbors[position][0]==header.from_node)
            {
                //position=i;
                debug_->debug("neighbor: %d\n",neighbors[position][0]);
                break;
            }
        }

        if (position!=NEIGHBOR_SIZE)                        //psaxnei na brei an to i diergasia pou proorizete to dao uparxei sto rout table. an oxi, tin prosthetei.
        {
            uint16_t position_2;
            uint16_t temp=0xffff;
            for (position_2=0;position_2<NEIGHBOR_SIZE; position_2++)
            {
                if(neighbors[position][position_2]==0xffff && temp==0xffff)
                {
                    //neighbors[position][position_2]=input_dao.from;
                   // break;
                   temp=position_2;
                }
                else if (neighbors[position][position_2]==input_dao.from)
                {
                    //debug_->debug("neighbors[position][position_2]: %d\n",neighbors[position][position_2]);
                    break;
                }
            }

            if (position_2==NEIGHBOR_SIZE)
            {
                 neighbors[position][temp]=input_dao.from;
                 debug_->debug("added: %d in position: %d and postion_2: %d\n",neighbors[position][temp],position,temp);
            }
        }

        dao_output(rpl_dag_structure, input_dao.to, input_dao.from);
   // }
}
//--------------------------------------------------------------------------------------------------------------------
void send_dao( void* )
{
    dao_output(rpl_dag_structure, 0, radio_->id());
}
//--------------------------------------------------------------------------------------------------------------------

uint16_t min_high_tree_OF(metrics_t input_metrics)
{
    //debug_->debug("metric1 %d\n", input_metrics.metric1);
    if (input_metrics.metric1==0xffff)
    return input_metrics.metric1;
    else
    return input_metrics.metric1+1;
}
//--------------------------------------------------------------------------------------------------------------------
    void receive_radio_message( Os::Radio::node_id_t from, Os::Radio::size_t len, Os::Radio::block_data_t *buf )
    {
        header_t header;
        memcpy(&header, buf, sizeof(header_t));

        switch(header.type)
        {
        case NEIGHBOR_DIS:
            handle_neighbor_discovery(header);
            // free(header);
            //TODO: complete this
            break;
        case NEIGHBOR_DIS_ACK:
            if(header.to_node==radio_->id())
            {
                handle_neighbor_discovery_ack(header);
            }
            break;
        case DIO:
            rpl_dio_t temp_dio;
            memcpy(&temp_dio, buf+sizeof(header_t), sizeof(rpl_dio_t));
            dio_input(temp_dio);
            break;
        case DAO:
            if(header.to_node==radio_->id())
            {
                debug_->debug("node: %d received dao from: %d\n",radio_->id(),header.from_node);
                rpl_dao_t temp_dao;
                memcpy(&temp_dao, buf+sizeof(header_t), sizeof(rpl_dao_t));
                dao_input(header, temp_dao);
            }
            break;
        }
    }
//--------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------
private:
        Os::Radio::self_pointer_t radio_;
        Os::Timer::self_pointer_t timer_;
        Os::Debug::self_pointer_t debug_;

        uint16_t neighbors[NEIGHBOR_SIZE][NEIGHBOR_SIZE];
        uint8_t hops_;
        uint16_t temp_rank;
        rpl_dag_t rpl_dag_structure;
        uint8_t sequence_counter_;
        uint8_t sent_timer_[SENT_QUEUE];
    };
// --------------------------------------------------------------------------
    wiselib::WiselibApplication<Os, rpl> rpl_app;
// --------------------------------------------------------------------------
    void application_main( Os::AppMainParameter& value )
    {
        rpl_app.init( value );
    }
