import os
import glob

def check_project():
    issues = []
    
    # Check for headers without .cpp files
    headers = glob.glob("src/**/*.h", recursive=True)
    for header in headers:
        cpp_file = header.replace('.h', '.cpp')
        if not os.path.exists(cpp_file):
            issues.append(f"Missing implementation: {header}")
    
    # Check for required JUCE includes
    for header in headers:
        with open(header, 'r') as f:
            content = f.read()
            if '#include <JuceHeader.h>' not in content:
                issues.append(f"Missing JuceHeader.h in: {header}")
    
    return issues

if __name__ == "__main__":
    problems = check_project()
    for problem in problems:
        print(f"⚠️  {problem}")