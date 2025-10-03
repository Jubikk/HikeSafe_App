import { useState, useEffect } from 'react';
import { insertUserData } from '../database/database';
import AsyncStorage from '@react-native-async-storage/async-storage';

export const useAppFlow = () => {
  const [showOnboarding, setShowOnboarding] = useState(true);
  const [showLogin, setShowLogin] = useState(false);
  const [showUserInfo, setShowUserInfo] = useState(false);
  const [showDashboard, setShowDashboard] = useState(false);
  const [showChannel, setShowChannel] = useState(false);
  const [showHistory, setShowHistory] = useState(false);
  const [showMap, setShowMap] = useState(false);
  const [showMessaging, setShowMessaging] = useState(false);
  const [showBluetoothConnection, setShowBluetoothConnection] = useState(false);
  const [bluetoothSkipped, setBluetoothSkipped] = useState(false);
  const [isScanning, setIsScanning] = useState(false);
  const [inputText, setInputText] = useState('');
  const [availableDevices, setAvailableDevices] = useState([]);

  // Reset and force show Bluetooth connection first
  useEffect(() => {
    const resetAndShowBluetooth = async () => {
      try {
        // Reset the skip status
        await AsyncStorage.setItem('bluetoothSkipped', 'false');
        console.log('Reset Bluetooth skip status to false');
        
        // Force show Bluetooth connection screen
        setShowBluetoothConnection(true);
        setShowOnboarding(false);
        setBluetoothSkipped(false);
        
      } catch (error) {
        console.error('Error resetting Bluetooth state:', error);
        // Ensure Bluetooth screen shows even if there's an error
        setShowBluetoothConnection(true);
        setShowOnboarding(false);
      }
    };
    
    // Run the reset
    resetAndShowBluetooth();
  }, []);

  // Onboarding completion
  const handleOnboardingComplete = async (userData) => {
    try {
      await insertUserData(userData);
      setShowOnboarding(false);
      setShowLogin(true);
      console.log('Onboarding completed');
    } catch (err) {
      console.error('Failed to save onboarding data', err);
    }
  };

  // Login completion - show Bluetooth connection or dashboard
  const handleLoginComplete = async (userData) => {
    try {
      if (userData) {
        await insertUserData(userData);
      }
      setShowLogin(false);
      
      // Only show Bluetooth connection if not previously skipped
      if (!bluetoothSkipped) {
        setShowBluetoothConnection(true);
      } else {
        setShowDashboard(true);
      }
      setShowUserInfo(false);
      console.log('Login completed, showing Bluetooth connection');
    } catch (err) {
      console.error('Failed to save login data', err);
    }
  };

  // Skip Bluetooth connection
  const skipBluetoothConnection = async () => {
    try {
      await AsyncStorage.setItem('bluetoothSkipped', 'true');
      setBluetoothSkipped(true);
      setShowBluetoothConnection(false);
      setShowLogin(true); // Go to login after skipping Bluetooth
    } catch (error) {
      console.error('Error saving Bluetooth skip status:', error);
    }
  };

  // Complete Bluetooth connection
  const completeBluetoothConnection = () => {
    setShowBluetoothConnection(false);
    setShowLogin(true); // Go to login after successful connection
  };

  // UserInfo completion - return to dashboard
  const handleUserInfoComplete = () => {
    console.log('UserInfo completed');
    setShowUserInfo(false);
    setShowDashboard(true);
  };

  // Helper function to reset all screen states
  const resetAllScreenStates = () => {
    setShowDashboard(false);
    setShowUserInfo(false);
    setShowChannel(false);
    setShowHistory(false);
    setShowMap(false);
    setShowMessaging(false);
    setShowBluetoothConnection(false);
  };

  // Navigation functions for each screen
  const navigateToDashboard = () => {
    resetAllScreenStates();
    setShowDashboard(true);
  };

  const navigateToUserInfo = () => {
    resetAllScreenStates();
    setShowUserInfo(true);
  };

  const navigateToChannel = () => {
    resetAllScreenStates();
    setShowChannel(true);
  };

  const navigateToHistory = () => {
    resetAllScreenStates();
    setShowHistory(true);
  };

  const navigateToMap = () => {
    resetAllScreenStates();
    setShowMap(true);
  };

  const navigateToMessaging = () => {
    resetAllScreenStates();
    setShowMessaging(true);
  };

  const navigateToBluetoothConnection = () => {
    resetAllScreenStates();
    setShowBluetoothConnection(true);
  };

  // Generic navigation handler
  const navigateToScreen = (screen) => {
    // Reset all screen states first
    setShowDashboard(false);
    setShowUserInfo(false);
    setShowChannel(false);
    setShowHistory(false);
    setShowMap(false);
    setShowMessaging(false);
    setShowBluetoothConnection(false);

    // Set the requested screen
    switch (screen) {
      case 'dashboard':
        setShowDashboard(true);
        break;
      case 'userInfo':
        setShowUserInfo(true);
        break;
      case 'channel':
        setShowChannel(true);
        break;
      case 'history':
        setShowHistory(true);
        break;
      case 'map':
        setShowMap(true);
        break;
      case 'messaging':
        setShowMessaging(true);
        break;
      case 'bluetoothConnection':
        setShowBluetoothConnection(true);
        break;
      default:
        console.warn(`Unknown screen: ${screen}`);
        setShowDashboard(true);
    }
  };

  // Reset all navigation states (useful for BLE mode)
  const resetNavigationStates = () => {
    setShowOnboarding(false);
    setShowLogin(false);
    setShowUserInfo(false);
    setShowDashboard(false);
    setShowChannel(false);
    setShowHistory(false);
    setShowMap(false);
    setShowMessaging(false);
    setShowBluetoothConnection(false);
  };

  return {
    // State flags
    showOnboarding,
    showLogin,
    showUserInfo,
    showDashboard,
    showChannel,
    showHistory,
    showMap,
    showMessaging,
    showBluetoothConnection,
    bluetoothSkipped,
    isScanning,
    availableDevices,
    setAvailableDevices,
    handleOnboardingComplete,
    handleLoginComplete,
    handleUserInfoComplete,
    navigateToScreen,
    // Manual navigation (legacy)
    setShowDashboard,
    setShowUserInfo,
    setShowChannel,
    setShowHistory,
    setShowMap,
    
    // State setters (for direct manipulation if needed)
    setShowOnboarding,
    setShowLogin,
    resetNavigationStates,
    
    // Bluetooth functions
    skipBluetoothConnection,
    completeBluetoothConnection,
  };
};