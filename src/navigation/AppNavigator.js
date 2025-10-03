// src/navigation/AppNavigator.js
import React from 'react';
import { SafeAreaView, Linking } from 'react-native';
import { useAppContext } from '../providers/AppProvider';

import OnboardingFlow from '../pages/Onboarding';
import HikingLoginScreen from '../pages/Login';
import Dashboard from '../pages/Dashboard';
import UserInfo from '../pages/userInfo';
import Channel from '../pages/Channel';
import History from '../pages/History';
import Map from '../pages/Map';
import InitializationScreen from '../pages/Bluetooth/InitializationScreen';
import ConnectionScreen from '../pages/Bluetooth/ConnectionScreen';
import ChatScreen from '../pages/Messaging/ChatScreen';
import styles from '../styles/AppStyles';

export default function AppNavigator() {
  const {
    showOnboarding,
    showLogin,
    showUserInfo,
    showDashboard,
    showChannel,
    showHistory,
    showMap,
    showMessaging,
    showBluetoothConnection,
    handleOnboardingComplete,
    handleLoginComplete,
    navigateToScreen,
    resetNavigationStates,
    manager,
    permissionsGranted,
    bleState,
    isConnected,
    isScanning,
    setIsScanning,
    availableDevices,
    setAvailableDevices,
    inputText,
    setInputText,
    debugInfo,
    messages,
    device,
    meshStatus,
    connectToDevice,
    disconnect,
    clearMessages,
    addMessage,
    addDebugInfo,
    bleManagerRef,
    skipBluetoothConnection,
    completeBluetoothConnection,
  } = useAppContext();

  const handleNavigate = (screen) => {
    navigateToScreen(screen);
  };

  // Bind the skip function to ensure proper 'this' context
  const handleSkipBluetooth = async () => {
    console.log('Skipping Bluetooth connection...');
    try {
      await skipBluetoothConnection();
    } catch (error) {
      console.error('Error in skipBluetoothConnection:', error);
      addDebugInfo(`Skip error: ${error.message}`);
    }
  };

  // Helper function to get current screen for bottom nav
  const getCurrentScreen = () => {
    if (showOnboarding) return 'onboarding';
    if (showLogin) return 'login';
    if (showDashboard) return 'dashboard';
    if (showUserInfo) return 'userInfo';
    if (showChannel) return 'channel';
    if (showHistory) return 'history';
    if (showMap) return 'map';
    if (showMessaging) return 'messaging';
    return 'unknown';
  };

  // Show Bluetooth connection screen first if not skipped
  if (showBluetoothConnection) {
    return (
      <SafeAreaView style={styles.container}>
        <ConnectionScreen
          bleState={bleState}
          isConnected={isConnected}
          isScanning={isScanning}
          setIsScanning={setIsScanning}
          availableDevices={availableDevices}
          setAvailableDevices={setAvailableDevices}
          debugInfo={debugInfo}
          messages={messages}
          connectToDevice={connectToDevice}
          clearMessages={clearMessages}
          bleManagerRef={bleManagerRef}
          addDebugInfo={addDebugInfo}
          onSkip={handleSkipBluetooth}
          onConnect={completeBluetoothConnection}
        />
      </SafeAreaView>
    );
  }

  if (showOnboarding) return <OnboardingFlow onComplete={handleOnboardingComplete} />;
  if (showLogin) return <HikingLoginScreen onLoginComplete={handleLoginComplete} />;

  // Main app screens with bottom navigation
  if (showDashboard || showUserInfo || showChannel || showHistory || showMap || showMessaging) {
    return (
      <SafeAreaView style={styles.container}>
        {showDashboard && (
          <Dashboard 
            currentScreen={getCurrentScreen()} 
            onNavigate={handleNavigate} 
          />
        )}
        {showUserInfo && (
          <UserInfo 
            currentScreen={getCurrentScreen()} 
            onComplete={() => navigateToScreen('dashboard')} 
            onNavigate={handleNavigate}
            onNavigateToBLE={handleNavigateToBLE}
          />
        )}
        {showChannel && (
          <Channel 
            currentScreen={getCurrentScreen()} 
            onNavigate={handleNavigate} 
          />
        )}
        {showHistory && (
          <History 
            currentScreen={getCurrentScreen()} 
            onNavigate={handleNavigate} 
          />
        )}
        {showMap && (
          <Map 
            currentScreen={getCurrentScreen()} 
            onNavigate={handleNavigate} 
          />
        )}
        {showMessaging && (
          <ChatScreen
            device={device}
            isConnected={isConnected}
            messages={messages}
            inputText={inputText}
            setInputText={setInputText}
            meshStatus={meshStatus}
            onDisconnect={disconnect}
            addMessage={addMessage}
            addDebugInfo={addDebugInfo}
            navigation={{ 
              goBack: () => navigateToScreen('dashboard') 
            }}
          />
        )}
      </SafeAreaView>
    );
  }

  // BLE initialization screen
  if (!manager || !permissionsGranted) {
    return (
      <SafeAreaView style={styles.container}>
        <InitializationScreen
          bleState={bleState}
          debugInfo={debugInfo}
          onOpenSettings={() => Linking.openSettings()}
        />
      </SafeAreaView>
    );
  }

  // BLE connection screen
  if (!isConnected) {
    return (
      <SafeAreaView style={styles.container}>
        <ConnectionScreen
          bleState={bleState}
          isScanning={isScanning}
          setIsScanning={setIsScanning}
          availableDevices={availableDevices}
          setAvailableDevices={setAvailableDevices}
          debugInfo={debugInfo}
          messages={messages}
          connectToDevice={(deviceId) => connectToDevice(deviceId, messages)}
          clearMessages={clearMessages}
          bleManagerRef={bleManagerRef}
          addDebugInfo={addDebugInfo}
        />
      </SafeAreaView>
    );
  }

  // BLE chat screen
  return (
    <SafeAreaView style={styles.container}>
      <ChatScreen
        device={device}
        isConnected={isConnected}
        messages={messages}
        inputText={inputText}
        setInputText={setInputText}
        meshStatus={meshStatus}
        onDisconnect={disconnect}
        addMessage={addMessage}
        addDebugInfo={addDebugInfo}
      />
    </SafeAreaView>
  );
}