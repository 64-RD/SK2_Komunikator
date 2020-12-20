import PySimpleGUI as sg


def main():
    # All the stuff inside your window.
    layout = [[sg.Text('It just works :)')],
              [sg.Button('God damn it Todd')]]

    # Create the Window
    window = sg.Window('Window Title', layout)

    # Event Loop to process "events" and get the "values" of the inputs
    while True:
        event, values = window.read()
        if event == sg.WIN_CLOSED or event == 'God damn it Todd':  # if user closes window or clicks cancel
            break

    window.close()
    return


if __name__ == "__main__":
    main()
