#pragma once

/*
 Config 은 Server Config과 Application Config으로 구성되며 XML 형식으로 작성된다.

 # server.conf

 	<?xml version="1.0" encoding="UTF-8" standalone="no"?>
 	<Server>
 		<Name>OvenMediaEngine</Name>
 		<Address>192.168.0.10</Address> <!-- 모든 IP 허용시 * | , 로 Multiple IP 설정 가능 -->
 		<Port>1935</Port>	<!-- , 로 Multiple Port 설정 가능 -->
 		<MaxConnection>0</MaxConnection> <!-- 0일경우 무제한 -->
 		<RootDir>${HOME}</RootDir>
 		<!-- 향후 Log, API, Stat 등 서버 일반적인 설정 추가 -->
 	</Server>

 	<!-- VHosts는 사용하지 않으면 생략 가능하다. -->
 	<VHosts>
 		<VHost>
 			<Name>OvenCloud</Name>
 			<!--
 			기본 설정, 다른 VHost와 IP, Port가 완전히 분리되어야 한다.
 			만약 Defalut가 * 이라면 다음 VHost는 충돌하기 때문에 오류가 발생한다.
 			-->
 			<Address>192.168.0.11</Address>
 			<Port>1935</Port>
 			<MaxConnection>100</MaxConnection>
 			<RootDir>${HOME}/OvenCloud</RootDir>
 		</VHost>
 		<VHost> <!-- 기본 설정, 다른 VHost와 IP, Port가 완전히 분리되어야 한다. -->
 			<Name>OvenAsset</Name>
 			<Address>192.168.0.11</Address>
 			<Port>1936</Port>
 			<MaxConnection>100</MaxConnection>
 			<RootDir>${HOME}/OvenAsset</RootDir>
 		</VHost>
	</VHosts>

 */

#include "items/items.h"
#include "config_manager.h"