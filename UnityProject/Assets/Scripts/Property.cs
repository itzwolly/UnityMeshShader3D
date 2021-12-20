public class Property {
    private string _type;
    private string _name;

    public Property(string pType, string pName) {
        _type = pType;
        _name = pName;
    }

    public string GetValueType() {
        return _type;
    }

    public string getName() {
        return _name;
    }
}