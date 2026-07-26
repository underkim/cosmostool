#pragma once

#include "services/doctor/IDoctorService.h"

#include <gmock/gmock.h>

namespace OpenC3::Tests {

/// gMock double for Services::IDoctorService.
class MockDoctorService : public Services::IDoctorService {
public:
    MOCK_METHOD(Models::DoctorReport, runAll,
                (std::function<void(const Models::HealthCheckResult&)>), (override));

    MOCK_METHOD(Models::HealthCheckResult, runCheck, (const std::string&), (override));
};

} // namespace OpenC3::Tests
