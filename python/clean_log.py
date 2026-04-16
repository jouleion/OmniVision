def clean_log(input_file, output_file):
    with open(input_file, 'r') as f:
        lines = f.readlines()

    cleaned_lines = []
    current_block = []
    in_block = False

    def extract_content(line):
        parts = line.split(',', 2)
        return parts[2] if len(parts) >= 3 else line

    for line in lines:
        content = extract_content(line)

        if "FREQ=" in content:
            if current_block:
                if not any("DONE: 0 measurements collected" in l for l in current_block):
                    cleaned_lines.extend(current_block)

            current_block = [content]
            in_block = True

        elif in_block:
            current_block.append(content)

            if "DONE:" in content:
                if "DONE: 0 measurements collected" not in content:
                    cleaned_lines.extend(current_block)

                current_block = []
                in_block = False

        else:
            cleaned_lines.append(content)

    with open(output_file, 'w') as f:
        f.writelines(cleaned_lines)


clean_log("serial_log_20260408_124538.csv", "cleaned_log.csv")
