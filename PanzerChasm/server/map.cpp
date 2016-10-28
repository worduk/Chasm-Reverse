#include <matrix.hpp>

#include "../math_utils.hpp"

#include "map.hpp"

namespace PanzerChasm
{

static const float g_commands_coords_scale= 1.0f / 256.0f;

Map::Map( const MapDataConstPtr& map_data )
	: map_data_(map_data)
{
	PC_ASSERT( map_data_ );

	procedures_.resize( map_data_->procedures.size() );

	dynamic_walls_.resize( map_data_->dynamic_walls.size() );
}

Map::~Map()
{}

void Map::ProcessPlayerPosition( const TimePoint current_time, const m_Vec3& pos )
{
	const unsigned int x= static_cast<unsigned int>( pos.x );
	const unsigned int y= static_cast<unsigned int>( pos.y );
	if( x >= MapData::c_map_size ||
		y >= MapData::c_map_size )
		return;

	const MapData::Link& link= map_data_->links[ x + y * MapData::c_map_size ];
	if( link.type == MapData::Link::Floor )
	{
		PC_ASSERT( link.proc_id < procedures_.size() );

		ProcedureState& procedure_state= procedures_[ link.proc_id ];
		if( procedure_state.movement_state == ProcedureState::MovementState::None )
		{
			procedure_state.movement_stage= 0.0f;
			procedure_state.movement_state= ProcedureState::MovementState::Movement;
			procedure_state.last_state_change_time= current_time;
		}
	}
}

void Map::Tick( const TimePoint current_time, const TimeInterval frame_delta )
{
	// Update state of procedures
	for( unsigned int p= 0u; p < procedures_.size(); p++ )
	{
		const MapData::Procedure& procedure= map_data_->procedures[p];
		ProcedureState& procedure_state= procedures_[p];

		const float stage_delta= frame_delta * procedure.speed / 10.0f;
		const float new_stage= procedure_state.movement_stage + stage_delta;

		switch( procedure_state.movement_state )
		{
		case ProcedureState::MovementState::None:
			break;

		case ProcedureState::MovementState::Movement:
			if( new_stage >= 1.0f )
			{
				procedure_state.movement_state= ProcedureState::MovementState::BackWait;
				procedure_state.movement_stage= 0.0f;
				procedure_state.last_state_change_time= current_time;
			}
			else
				procedure_state.movement_stage= new_stage;
			break;

		case ProcedureState::MovementState::BackWait:
		{
			const TimeInterval wait_time= current_time - procedure_state.last_state_change_time;
			if(
				procedure.back_wait_s > 0.0f &&
				wait_time >= procedure.back_wait_s )
			{
				procedure_state.movement_state= ProcedureState::MovementState::ReverseMovement;
				procedure_state.movement_stage= 0.0f;
				procedure_state.last_state_change_time= current_time;
			}
		}
			break;

		case ProcedureState::MovementState::ReverseMovement:
			if( new_stage >= 1.0f )
			{
				procedure_state.movement_state= ProcedureState::MovementState::None;
				procedure_state.movement_stage= 0.0f;
				procedure_state.last_state_change_time= current_time;
			}
			else
				procedure_state.movement_stage= new_stage;
			break;
		}; // switch state


		// Select positions, using movement state.

		float absolute_action_stage;
		if( procedure_state.movement_state == ProcedureState::MovementState::Movement )
			absolute_action_stage= procedure_state.movement_stage;
		else if( procedure_state.movement_state == ProcedureState::MovementState::BackWait )
			absolute_action_stage= 1.0f;
		else if( procedure_state.movement_state == ProcedureState::MovementState::ReverseMovement )
			absolute_action_stage= 1.0f - procedure_state.movement_stage;
		else
			absolute_action_stage= 0.0f;

		for( const MapData::Procedure::ActionCommand& command : procedure.action_commands )
		{
			using Action= MapData::Procedure::ActionCommandId;
			switch( command.id )
			{
			case Action::Move:
			{
				const unsigned char x= static_cast<unsigned char>(command.args[0]);
				const unsigned char y= static_cast<unsigned char>(command.args[1]);
				const float dx= command.args[2] * g_commands_coords_scale;
				const float dy= command.args[3] * g_commands_coords_scale;
				const float sound_number= command.args[4];
				PC_UNUSED(sound_number);

				PC_ASSERT( x < MapData::c_map_size && y < MapData::c_map_size );
				const MapData::IndexElement& index_element= map_data_->map_index[ x + y * MapData::c_map_size ];

				if( index_element.type == MapData::IndexElement::DynamicWall )
				{
					PC_ASSERT( index_element.index < map_data_->dynamic_walls.size() );

					const MapData::Wall& map_wall= map_data_->dynamic_walls[ index_element.index ];
					DynamicWall& wall= dynamic_walls_[ index_element.index ];

					for( unsigned int v= 0u; v < 2u; v++ )
					{
						wall.vert_pos[v]= map_wall.vert_pos[v];
						wall.vert_pos[v].x+= dx * absolute_action_stage;
						wall.vert_pos[v].y+= dy * absolute_action_stage;
					}
					wall.z= 0.0f;
				}
			}
				break;

			case Action::Rotate:
			{
				const unsigned char x= static_cast<unsigned char>(command.args[0]);
				const unsigned char y= static_cast<unsigned char>(command.args[1]);
				const float center_x= command.args[2] * g_commands_coords_scale;
				const float center_y= command.args[3] * g_commands_coords_scale;
				const float angle= command.args[4] * Constants::to_rad;
				const float sound_number= command.args[5];
				PC_UNUSED(sound_number);

				PC_ASSERT( x < MapData::c_map_size && y < MapData::c_map_size );

				const MapData::IndexElement& index_element= map_data_->map_index[ x + y * MapData::c_map_size ];

				if( index_element.type == MapData::IndexElement::DynamicWall )
				{
					PC_ASSERT( index_element.index < map_data_->dynamic_walls.size() );

					const MapData::Wall& map_wall= map_data_->dynamic_walls[ index_element.index ];
					DynamicWall& wall= dynamic_walls_[ index_element.index ];

					m_Mat3 rot_mat;
					rot_mat.RotateZ( angle * absolute_action_stage );
					const m_Vec2 center( center_x, center_y );

					for( unsigned int v= 0u; v < 2u; v++ )
					{
						const m_Vec2 vec= map_wall.vert_pos[v] - center;
						const m_Vec2 vec_rotated= ( m_Vec3( vec, 0.0f ) * rot_mat ).xy();
						wall.vert_pos[v]= center + vec_rotated;
					}
					wall.z= 0.0f;
				}
			}
				break;

			case Action::Up:
			{
				const unsigned char x= static_cast<unsigned char>(command.args[0]);
				const unsigned char y= static_cast<unsigned char>(command.args[1]);
				const float height= command.args[2] * g_commands_coords_scale;
				const float sound_number= command.args[3];
				PC_UNUSED(sound_number);

				PC_ASSERT( x < MapData::c_map_size && y < MapData::c_map_size );
				const MapData::IndexElement& index_element= map_data_->map_index[ x + y * MapData::c_map_size ];

				if( index_element.type == MapData::IndexElement::DynamicWall )
				{
					PC_ASSERT( index_element.index < map_data_->dynamic_walls.size() );

					const MapData::Wall& map_wall= map_data_->dynamic_walls[ index_element.index ];
					DynamicWall& wall= dynamic_walls_[ index_element.index ];

					for( unsigned int v= 0u; v < 2u; v++ )
						wall.vert_pos[v]= map_wall.vert_pos[v];
					wall.z= height * absolute_action_stage;
				}
			}
				break;

			default:
				// TODO
				break;
			}
		} // for action commands

	} // for procedures

}

void Map::SendUpdateMessages( MessagesSender& messages_sender ) const
{
	Messages::WallPosition wall_message;
	wall_message.message_id= MessageId::WallPosition;

	for( const DynamicWall& wall : dynamic_walls_ )
	{
		wall_message.wall_index= &wall - dynamic_walls_.data();

		wall_message.vertices_xy[0][0]= short( wall.vert_pos[0].x * 256.0f );
		wall_message.vertices_xy[0][1]= short( wall.vert_pos[0].y * 256.0f );
		wall_message.vertices_xy[1][0]= short( wall.vert_pos[1].x * 256.0f );
		wall_message.vertices_xy[1][1]= short( wall.vert_pos[1].y * 256.0f );

		wall_message.z= short( wall.z * 256.0f );

		messages_sender.SendUnreliableMessage( wall_message );
	}
}

} // PanzerChasm
