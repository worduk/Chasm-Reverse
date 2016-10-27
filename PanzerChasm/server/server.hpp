#pragma once
#include <chrono>

#include "../connection_info.hpp"
#include "../game_resources.hpp"
#include "../map_loader.hpp"
#include "i_connections_listener.hpp"
#include "map.hpp"

namespace PanzerChasm
{

class Server final
{
public:
	Server(
		const GameResourcesConstPtr& game_resources,
		const MapLoaderPtr& map_loader,
		const IConnectionsListenerPtr& connections_listener );
	~Server();

	void Loop();

	void ChangeMap( unsigned int map_number );

public: // Messages handlers
	void operator()( const Messages::MessageBase& message );
	void operator()( const Messages::PlayerMove& message );

private:
	enum class State
	{
		NoMap,
		PlayingMap,
	};

private:
	const GameResourcesConstPtr game_resources_;
	const MapLoaderPtr map_loader_;
	const IConnectionsListenerPtr connections_listener_;

	State state_= State::NoMap;
	unsigned int current_map_number_= ~0;
	MapDataConstPtr current_map_data_;
	std::unique_ptr<Map> map_;

	std::unique_ptr<ConnectionInfo> connection_;

	const std::chrono::milliseconds startup_time_;
	std::chrono::milliseconds last_tick_;
	float last_tick_duration_s_;

	m_Vec3 player_pos_;
	struct
	{
		float acceleration; // 0.0f - 1.0f
		float direction;
	} player_movement_;
};

} // PanzerChasm
