# ============================================================================
# ROLLBACK TEST SCRIPT
# ============================================================================
# This script helps you test the rollback functionality of Firminia 3.6.1
# 
# Usage:
#   .\test_rollback.ps1 -Test 1    # Test corrupted firmware
#   .\test_rollback.ps1 -Test 2    # Test boot watchdog
#   .\test_rollback.ps1 -Test 3    # Test firmware validation
#   .\test_rollback.ps1 -All       # Run all tests
#   .\test_rollback.ps1 -Help      # Show help
# ============================================================================

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet(1, 2, 3, "All", "Help")]
    [string]$Test = "Help",
    
    [Parameter(Mandatory=$false)]
    [switch]$Monitor = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$Force = $false
)

# Configuration
$IDF_PATH = "C:\Users\biso\esp\v5.5\esp-idf\tools\idf.py"
$PROJECT_PATH = "C:\Users\biso\DEV\firminia3"
$BACKUP_FIRMWARE = "firmware_backup.bin"

# Colors for output
$Red = "Red"
$Green = "Green"
$Yellow = "Yellow"
$Cyan = "Cyan"

function Write-ColorOutput {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
}

function Show-Help {
    Write-ColorOutput "🧪 FIRMINIA ROLLBACK TEST SCRIPT" $Cyan
    Write-ColorOutput "=================================" $Cyan
    Write-ColorOutput ""
    Write-ColorOutput "Available Tests:" $Yellow
    Write-ColorOutput "  1  - Test Corrupted Firmware (CRASH TEST)" $Red
    Write-ColorOutput "  2  - Test Firmware Validation (WARNING TEST)" $Green
    Write-ColorOutput "  All - Run all tests sequentially" $Cyan
    Write-ColorOutput ""
    Write-ColorOutput "Usage Examples:" $Yellow
    Write-ColorOutput "  .\test_rollback.ps1 -Test 1" $White
    Write-ColorOutput "  .\test_rollback.ps1 -Test 2 -Monitor" $White
    Write-ColorOutput "  .\test_rollback.ps1 -Test 3 -Force" $White
    Write-ColorOutput ""
    Write-ColorOutput "Options:" $Yellow
    Write-ColorOutput "  -Monitor  : Start monitoring after test" $White
    Write-ColorOutput "  -Force    : Skip confirmation prompts" $White
    Write-ColorOutput ""
    Write-ColorOutput "⚠️  WARNING: Tests 1 and 2 will cause system crashes!" $Red
    Write-ColorOutput "   Make sure you have a working firmware backup!" $Red
}

function Test-Prerequisites {
    Write-ColorOutput "🔍 Checking prerequisites..." $Cyan
    
    # Check if IDF path exists
    if (-not (Test-Path $IDF_PATH)) {
        Write-ColorOutput "❌ ESP-IDF not found at: $IDF_PATH" $Red
        Write-ColorOutput "   Please update the IDF_PATH variable in the script" $Yellow
        return $false
    }
    
    # Check if project path exists
    if (-not (Test-Path $PROJECT_PATH)) {
        Write-ColorOutput "❌ Project not found at: $PROJECT_PATH" $Red
        Write-ColorOutput "   Please update the PROJECT_PATH variable in the script" $Yellow
        return $false
    }
    
    Write-ColorOutput "✅ Prerequisites check passed" $Green
    return $true
}

function Set-TestConfiguration {
    param([int]$TestNumber)
    
    Write-ColorOutput "⚙️  Configuring test $TestNumber..." $Cyan
    
    # Read current main_flow.c
    $mainFlowPath = Join-Path $PROJECT_PATH "main\main_flow.c"
    $content = Get-Content $mainFlowPath -Raw
    
    # Reset all test flags
    $content = $content -replace '#define ENABLE_ROLLBACK_TESTS\s+\d+', '#define ENABLE_ROLLBACK_TESTS          0'
    $content = $content -replace '#define TEST_FIRMWARE_CORRUPTION\s+\d+', '#define TEST_FIRMWARE_CORRUPTION       0'
    $content = $content -replace '#define TEST_PROBLEMATIC_FIRMWARE\s+\d+', '#define TEST_PROBLEMATIC_FIRMWARE      0'
    $content = $content -replace '#define TEST_FIRMWARE_VALIDATION\s+\d+', '#define TEST_FIRMWARE_VALIDATION      0'
    
    # Enable specific test
    switch ($TestNumber) {
        1 {
            $content = $content -replace '#define ENABLE_ROLLBACK_TESTS\s+0', '#define ENABLE_ROLLBACK_TESTS          1'
            $content = $content -replace '#define TEST_FIRMWARE_CORRUPTION\s+0', '#define TEST_FIRMWARE_CORRUPTION       1'
            Write-ColorOutput "✅ Test 1 configured: Corrupted Firmware" $Red
        }
        2 {
            $content = $content -replace '#define ENABLE_ROLLBACK_TESTS\s+0', '#define ENABLE_ROLLBACK_TESTS          1'
            $content = $content -replace '#define TEST_FIRMWARE_VALIDATION\s+0', '#define TEST_FIRMWARE_VALIDATION      1'
            Write-ColorOutput "✅ Test 2 configured: Firmware Validation" $Green
        }
    }
    
    # Write back to file
    Set-Content $mainFlowPath $content -NoNewline
    Write-ColorOutput "✅ Configuration updated" $Green
}

function Build-And-Flash {
    Write-ColorOutput "🔨 Building and flashing firmware..." $Cyan
    
    # Change to project directory
    Push-Location $PROJECT_PATH
    
    try {
        # Build
        Write-ColorOutput "Building..." $Yellow
        $buildResult = & python $IDF_PATH build 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "❌ Build failed!" $Red
            Write-ColorOutput $buildResult $Red
            return $false
        }
        
        # Flash
        Write-ColorOutput "Flashing..." $Yellow
        $flashResult = & python $IDF_PATH flash 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-ColorOutput "❌ Flash failed!" $Red
            Write-ColorOutput $flashResult $Red
            return $false
        }
        
        Write-ColorOutput "✅ Build and flash completed" $Green
        return $true
    }
    finally {
        Pop-Location
    }
}

function Start-Monitoring {
    Write-ColorOutput "📡 Starting monitoring..." $Cyan
    Write-ColorOutput "Press Ctrl+C to stop monitoring" $Yellow
    
    Push-Location $PROJECT_PATH
    try {
        & python $IDF_PATH monitor
    }
    finally {
        Pop-Location
    }
}

function Reset-TestConfiguration {
    Write-ColorOutput "🔄 Resetting test configuration..." $Cyan
    
    $mainFlowPath = Join-Path $PROJECT_PATH "main\main_flow.c"
    $content = Get-Content $mainFlowPath -Raw
    
    # Reset all test flags
    $content = $content -replace '#define ENABLE_ROLLBACK_TESTS\s+\d+', '#define ENABLE_ROLLBACK_TESTS          0'
    $content = $content -replace '#define TEST_FIRMWARE_CORRUPTION\s+\d+', '#define TEST_FIRMWARE_CORRUPTION       0'
    $content = $content -replace '#define TEST_PROBLEMATIC_FIRMWARE\s+\d+', '#define TEST_PROBLEMATIC_FIRMWARE      0'
    $content = $content -replace '#define TEST_FIRMWARE_VALIDATION\s+\d+', '#define TEST_FIRMWARE_VALIDATION      0'
    
    Set-Content $mainFlowPath $content -NoNewline
    Write-ColorOutput "✅ Test configuration reset" $Green
}

# Main script logic
if ($Test -eq "Help") {
    Show-Help
    exit 0
}

Write-ColorOutput "🧪 FIRMINIA ROLLBACK TEST SCRIPT" $Cyan
Write-ColorOutput "=================================" $Cyan

# Check prerequisites
if (-not (Test-Prerequisites)) {
    exit 1
}

# Show warning for dangerous tests
if ($Test -eq 1 -or $Test -eq 2 -or $Test -eq "All") {
    Write-ColorOutput "" $White
    Write-ColorOutput "⚠️  WARNING: This test will cause system crashes!" $Red
    Write-ColorOutput "   Make sure you have a working firmware backup!" $Red
    Write-ColorOutput "" $White
    
    if (-not $Force) {
        $confirm = Read-Host "Do you want to continue? (y/N)"
        if ($confirm -ne "y" -and $confirm -ne "Y") {
            Write-ColorOutput "Test cancelled by user" $Yellow
            exit 0
        }
    }
}

# Run tests
if ($Test -eq "All") {
    Write-ColorOutput "🚀 Running all tests..." $Cyan
    
    # Test 2 (safest first)
    Write-ColorOutput "`n🧪 Running Test 2: Firmware Validation" $Green
    Set-TestConfiguration 2
    if (Build-And-Flash) {
        Write-ColorOutput "✅ Test 2 completed - check logs for warnings" $Green
    }
    
    # Test 1 (most dangerous)
    Write-ColorOutput "`n🧪 Running Test 1: Corrupted Firmware" $Red
    Set-TestConfiguration 1
    if (Build-And-Flash) {
        Write-ColorOutput "✅ Test 1 completed - check for crash and rollback" $Red
    }
} else {
    $testNumber = [int]$Test
    Write-ColorOutput "🚀 Running Test $testNumber..." $Cyan
    
    Set-TestConfiguration $testNumber
    if (Build-And-Flash) {
        Write-ColorOutput "✅ Test $testNumber completed" $Green
    }
}

# Start monitoring if requested
if ($Monitor) {
    Start-Monitoring
}

# Reset configuration
Reset-TestConfiguration

Write-ColorOutput "`n🎉 Test script completed!" $Green
Write-ColorOutput "Check the logs for test results and rollback behavior." $Yellow
