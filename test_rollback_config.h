/**
 * @file test_rollback_config.h
 * @brief Configuration file for rollback testing
 * 
 * This file contains the configuration for rollback tests.
 * Modify the values below to enable/disable specific tests.
 */

#ifndef TEST_ROLLBACK_CONFIG_H
#define TEST_ROLLBACK_CONFIG_H

// ============================================================================
// ROLLBACK TEST CONFIGURATION
// ============================================================================

// Master switch for all rollback tests
#define ENABLE_ROLLBACK_TESTS          0       // Set to 1 to enable test functions

// Individual test switches
#define TEST_FIRMWARE_CORRUPTION       0       // Test 1: Corrupted firmware (CRASH TEST)
#define TEST_BOOT_WATCHDOG             0       // Test 2: Boot timeout (WATCHDOG TEST)
#define TEST_FIRMWARE_VALIDATION      0       // Test 3: Firmware validation (WARNING TEST)

// Test timing configuration
#define TEST_CRASH_DELAY_MS            2000    // Delay before crash in test 1
#define TEST_BOOT_SIMULATION_SEC       40      // Duration of boot simulation in test 2
#define TEST_WATCHDOG_TIMEOUT_SEC      30      // Expected watchdog timeout

// Test logging configuration
#define TEST_LOG_PREFIX                "🧪 TEST"
#define TEST_LOG_SEPARATOR             "========================================"

// ============================================================================
// TEST INSTRUCTIONS
// ============================================================================

/*
 * HOW TO USE THESE TESTS:
 * 
 * 1. TEST_FIRMWARE_CORRUPTION (Test 1):
 *    - Set ENABLE_ROLLBACK_TESTS = 1
 *    - Set TEST_FIRMWARE_CORRUPTION = 1
 *    - Compile and flash
 *    - Expected: System crashes, rollback occurs, system restarts with previous firmware
 *    - WARNING: This will cause a crash!
 * 
 * 2. TEST_BOOT_WATCHDOG (Test 2):
 *    - Set ENABLE_ROLLBACK_TESTS = 1
 *    - Set TEST_BOOT_WATCHDOG = 1
 *    - Compile and flash
 *    - Expected: Boot takes >30 seconds, watchdog triggers, rollback occurs
 *    - WARNING: This will take 30+ seconds to complete!
 * 
 * 3. TEST_FIRMWARE_VALIDATION (Test 3):
 *    - Set ENABLE_ROLLBACK_TESTS = 1
 *    - Set TEST_FIRMWARE_VALIDATION = 1
 *    - Compile and flash
 *    - Expected: Warnings in logs, but NO rollback occurs
 *    - SAFE: This test only shows warnings
 * 
 * SAFETY NOTES:
 * - Always test on a device that can be recovered
 * - Have a working firmware backup ready
 * - Monitor logs during testing
 * - Test one scenario at a time
 * - Disable tests when not needed
 */

#endif // TEST_ROLLBACK_CONFIG_H
